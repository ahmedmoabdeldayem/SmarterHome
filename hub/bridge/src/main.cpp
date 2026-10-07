/**
 * SmarterHome — MQTT Bridge (C++)
 *
 * Bridges the local Mosquitto broker ↔ AWS IoT Core bidirectionally.
 *
 *   Local Mosquitto (LAN, no TLS)  ←──── bridge ────→  AWS IoT Core (TLS/X.509)
 *
 * Messages published to local Mosquitto are forwarded to AWS IoT Core,
 * and vice versa. Echo-loop prevention ensures a forwarded message is
 * not re-forwarded back to its origin.
 *
 * Build:
 *   cd hub/bridge
 *   cmake -B build && cmake --build build -j$(nproc)
 *
 * Run:
 *   ./build/smarthome_bridge
 */

#include <mqtt/async_client.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "config.h"

// ── Logging helpers ───────────────────────────────────────────────────────────
#define LOG(level, msg) \
    std::cout << "[" level "] " << msg << std::endl

// ── Globals ───────────────────────────────────────────────────────────────────
static std::atomic<bool> g_running{true};

// Echo-loop prevention: track messages currently being forwarded
// Key = topic + "|" + payload
struct InFlight {
    std::mutex              mtx;
    std::unordered_set<std::string> keys;

    bool try_mark(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        return keys.insert(key).second;  // true = newly inserted (not a loop)
    }

    void unmark(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        keys.erase(key);
    }
};

static InFlight g_local_to_aws;
static InFlight g_aws_to_local;

// ── Forward declarations ──────────────────────────────────────────────────────
class LocalCallback;
class AwsCallback;

// ── Local broker callback ─────────────────────────────────────────────────────
class LocalCallback : public virtual mqtt::callback {
public:
    LocalCallback(mqtt::async_client& local, mqtt::async_client& aws)
        : local_(local), aws_(aws) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
        const std::string& topic   = msg->get_topic();
        const std::string& payload = msg->to_string();
        const std::string  key     = topic + "|" + payload;

        // If this message came from AWS (we're about to echo it back), skip it
        if (!g_aws_to_local.try_mark(key)) return;

        LOG("LOCAL→AWS", topic);

        // Mark as in-flight from local side
        g_local_to_aws.try_mark(key);

        try {
            aws_.publish(topic, payload, 1, false);
        } catch (const mqtt::exception& e) {
            LOG("ERROR", "Failed to forward to AWS: " << e.what());
        }

        g_aws_to_local.unmark(key);
    }

    void connected(const std::string&) override {
        LOG("INFO", "Connected to local Mosquitto broker");
        local_.subscribe(TOPIC_FILTER, 1);
    }

    void connection_lost(const std::string& cause) override {
        LOG("WARN", "Lost connection to local Mosquitto: " << cause);
    }

private:
    mqtt::async_client& local_;
    mqtt::async_client& aws_;
};

// ── AWS IoT Core callback ─────────────────────────────────────────────────────
class AwsCallback : public virtual mqtt::callback {
public:
    AwsCallback(mqtt::async_client& local, mqtt::async_client& aws)
        : local_(local), aws_(aws) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
        const std::string& topic   = msg->get_topic();
        const std::string& payload = msg->to_string();
        const std::string  key     = topic + "|" + payload;

        // If this message came from local (we're about to echo it back), skip it
        if (!g_local_to_aws.try_mark(key)) return;

        LOG("AWS→LOCAL", topic);

        g_aws_to_local.try_mark(key);

        try {
            local_.publish(topic, payload, 1, false);
        } catch (const mqtt::exception& e) {
            LOG("ERROR", "Failed to forward to local: " << e.what());
        }

        g_local_to_aws.unmark(key);
    }

    void connected(const std::string&) override {
        LOG("INFO", "Connected to AWS IoT Core");
        aws_.subscribe(TOPIC_FILTER, 1);

        // Publish hub online status
        aws_.publish(HUB_STATUS_TOPIC, R"({"online":true})", 1, true);
    }

    void connection_lost(const std::string& cause) override {
        LOG("WARN", "Lost connection to AWS IoT Core: " << cause);
    }

private:
    mqtt::async_client& local_;
    mqtt::async_client& aws_;
};

// ── Build SSL options for AWS IoT Core ───────────────────────────────────────
static mqtt::ssl_options build_aws_ssl() {
    mqtt::ssl_options ssl;
    ssl.set_trust_store(AWS_CA_CERT);
    ssl.set_key_store(AWS_DEVICE_CERT);
    ssl.set_private_key(AWS_PRIVATE_KEY);
    ssl.set_verify(true);
    return ssl;
}

// ── Connect with retry ────────────────────────────────────────────────────────
static void connect_with_retry(mqtt::async_client& client,
                                mqtt::connect_options& opts,
                                const char* name) {
    while (g_running) {
        try {
            LOG("INFO", "Connecting to " << name << "...");
            client.connect(opts)->wait();
            return;
        } catch (const mqtt::exception& e) {
            LOG("ERROR", "Connection to " << name << " failed: " << e.what()
                << " — retrying in " << RECONNECT_DELAY_MS << "ms");
            std::this_thread::sleep_for(
                std::chrono::milliseconds(RECONNECT_DELAY_MS));
        }
    }
}

// ── Signal handler ────────────────────────────────────────────────────────────
static void on_signal(int) {
    LOG("INFO", "Shutting down bridge...");
    g_running = false;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // ── Local client ──────────────────────────────────────────────
    mqtt::async_client local_client(LOCAL_BROKER_URI, LOCAL_CLIENT_ID);

    mqtt::connect_options local_opts;
    local_opts.set_keep_alive_interval(KEEPALIVE_SECONDS);
    local_opts.set_clean_session(false);
    local_opts.set_automatic_reconnect(true);

    // ── AWS client ────────────────────────────────────────────────
    const std::string aws_uri = std::string("ssl://") + AWS_ENDPOINT
                                + ":" + std::to_string(AWS_PORT);
    mqtt::async_client aws_client(aws_uri, AWS_CLIENT_ID);

    mqtt::connect_options aws_opts;
    aws_opts.set_keep_alive_interval(KEEPALIVE_SECONDS);
    aws_opts.set_clean_session(false);
    aws_opts.set_automatic_reconnect(true);
    aws_opts.set_ssl(build_aws_ssl());

    // LWT: mark hub offline if connection drops unexpectedly
    auto lwt = mqtt::message::create(HUB_STATUS_TOPIC, R"({"online":false})", 1, true);
    aws_opts.set_will_message(lwt);

    // ── Attach callbacks ──────────────────────────────────────────
    LocalCallback local_cb(local_client, aws_client);
    AwsCallback   aws_cb(local_client, aws_client);

    local_client.set_callback(local_cb);
    aws_client.set_callback(aws_cb);

    // ── Connect both brokers ──────────────────────────────────────
    connect_with_retry(local_client, local_opts, "local Mosquitto");
    connect_with_retry(aws_client,   aws_opts,   "AWS IoT Core");

    LOG("INFO", "Bridge running. Press Ctrl+C to stop.");

    // ── Main loop: reconnect if either drops ──────────────────────
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (!local_client.is_connected()) {
            LOG("WARN", "Local broker disconnected, reconnecting...");
            connect_with_retry(local_client, local_opts, "local Mosquitto");
        }
        if (!aws_client.is_connected()) {
            LOG("WARN", "AWS IoT Core disconnected, reconnecting...");
            connect_with_retry(aws_client, aws_opts, "AWS IoT Core");
        }
    }

    // ── Graceful shutdown ─────────────────────────────────────────
    try {
        aws_client.publish(HUB_STATUS_TOPIC, R"({"online":false})", 1, true)->wait();
        local_client.disconnect()->wait();
        aws_client.disconnect()->wait();
    } catch (const mqtt::exception& e) {
        LOG("ERROR", "Disconnect error: " << e.what());
    }

    LOG("INFO", "Bridge stopped.");
    return 0;
}

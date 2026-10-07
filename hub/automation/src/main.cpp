/**
 * SmarterHome — Local Automation Engine (C++)
 *
 * Runs on the Raspberry Pi. Subscribes to sensor topics on the local
 * Mosquitto broker and fires automation rules without needing AWS —
 * so the home still works during internet outages.
 *
 * Rules are defined in rules.h. Add or remove rules there.
 *
 * Build (from hub/):
 *   cmake -B build && cmake --build build -j$(nproc)
 *
 * Run:
 *   ./build/smarthome_automation
 */

#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "rules.h"

using json = nlohmann::json;

// ── Config ────────────────────────────────────────────────────────────────────
static constexpr const char* BROKER_URI  = "tcp://localhost:1883";
static constexpr const char* CLIENT_ID   = "smarthome-automation";
static constexpr int         KEEPALIVE   = 30;
static constexpr int         RECONNECT_MS = 5000;

#define LOG(level, msg) \
    std::cout << "[" level "] " << msg << std::endl

// ── Globals ───────────────────────────────────────────────────────────────────
static std::atomic<bool> g_running{true};
static std::vector<Rule> g_rules = build_rules();

// topic → indices of rules that watch it
static std::unordered_map<std::string, std::vector<size_t>> g_topic_index;

static void build_index() {
    for (size_t i = 0; i < g_rules.size(); ++i)
        g_topic_index[g_rules[i].trigger_topic].push_back(i);
}

// ── Automation callback ───────────────────────────────────────────────────────
class AutomationCallback : public virtual mqtt::callback {
public:
    explicit AutomationCallback(mqtt::async_client& client)
        : client_(client) {}

    void connected(const std::string&) override {
        LOG("INFO", "Connected to local Mosquitto broker");
        for (const auto& [topic, _] : g_topic_index) {
            client_.subscribe(topic, 1);
            LOG("INFO", "Subscribed to: " << topic);
        }
    }

    void connection_lost(const std::string& cause) override {
        LOG("WARN", "Connection lost: " << cause);
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        const std::string& topic = msg->get_topic();

        auto it = g_topic_index.find(topic);
        if (it == g_topic_index.end()) return;

        // Parse payload
        json payload;
        try {
            payload = json::parse(msg->to_string());
        } catch (...) {
            return;
        }

        auto now = std::chrono::steady_clock::now();

        for (size_t idx : it->second) {
            Rule& rule = g_rules[idx];

            // Check condition
            bool should_fire = false;
            try {
                should_fire = rule.condition(payload);
            } catch (...) {
                continue;
            }
            if (!should_fire) continue;

            // Check cooldown
            if (now - rule.last_fired < rule.cooldown) continue;
            rule.last_fired = now;

            // Build action payload
            json action;
            try {
                action = rule.action_payload(payload);
            } catch (...) {
                continue;
            }

            // Publish action
            try {
                client_.publish(rule.action_topic, action.dump(), 1, false);
                LOG("RULE", rule.description);
            } catch (const mqtt::exception& e) {
                LOG("ERROR", "Publish failed: " << e.what());
            }
        }
    }

private:
    mqtt::async_client& client_;
};

// ── Signal handler ────────────────────────────────────────────────────────────
static void on_signal(int) {
    LOG("INFO", "Shutting down automation engine...");
    g_running = false;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    build_index();

    mqtt::async_client client(BROKER_URI, CLIENT_ID);

    mqtt::connect_options opts;
    opts.set_keep_alive_interval(KEEPALIVE);
    opts.set_clean_session(false);
    opts.set_automatic_reconnect(true);

    AutomationCallback cb(client);
    client.set_callback(cb);

    // Connect with retry
    while (g_running) {
        try {
            LOG("INFO", "Connecting to local Mosquitto...");
            client.connect(opts)->wait();
            break;
        } catch (const mqtt::exception& e) {
            LOG("ERROR", "Connection failed: " << e.what()
                << " — retrying in " << RECONNECT_MS << "ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_MS));
        }
    }

    LOG("INFO", "Automation engine running with "
        << g_rules.size() << " rules. Press Ctrl+C to stop.");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!client.is_connected() && g_running) {
            LOG("WARN", "Disconnected, reconnecting...");
            try {
                client.reconnect()->wait();
            } catch (...) {}
        }
    }

    try {
        client.disconnect()->wait();
    } catch (...) {}

    LOG("INFO", "Automation engine stopped.");
    return 0;
}

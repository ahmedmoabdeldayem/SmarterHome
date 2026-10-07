#pragma once

// ── Local Mosquitto broker ────────────────────────────────────────────────────
static constexpr const char* LOCAL_BROKER_URI  = "tcp://localhost:1883";
static constexpr const char* LOCAL_CLIENT_ID   = "smarthome-bridge-local";

// ── AWS IoT Core ──────────────────────────────────────────────────────────────
// Fill in your endpoint from aws/certs/endpoint.txt after running provision.py
static constexpr const char* AWS_ENDPOINT      = "XXXXXXXXXXXX-ats.iot.us-east-1.amazonaws.com";
static constexpr int         AWS_PORT          = 8883;
static constexpr const char* AWS_CLIENT_ID     = "smarthome-hub";

// Certificate paths (copy from aws/certs/smarthome-hub/ to hub/certs/)
static constexpr const char* AWS_CA_CERT       = "../certs/AmazonRootCA1.pem";
static constexpr const char* AWS_DEVICE_CERT   = "../certs/certificate.pem.crt";
static constexpr const char* AWS_PRIVATE_KEY   = "../certs/private.pem.key";

// ── Topic filter ──────────────────────────────────────────────────────────────
static constexpr const char* TOPIC_FILTER      = "smarthome/#";
static constexpr const char* HUB_STATUS_TOPIC  = "smarthome/hub/status";

// ── Reconnect settings ────────────────────────────────────────────────────────
static constexpr int RECONNECT_DELAY_MS        = 5000;
static constexpr int KEEPALIVE_SECONDS         = 30;

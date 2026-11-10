#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ── Configuration ─────────────────────────────────────────────────────────────
// Copy secrets.h.template → secrets.h, fill in your values, do NOT commit secrets.h
#include "secrets.h"

#define MQTT_PORT 1883

// ── Shared helpers ────────────────────────────────────────────────────────────

/**
 * Connect to Wi-Fi. Blocks until connected.
 * Call once from setup().
 */
void wifi_connect();

/**
 * Re-connect to Wi-Fi if the connection has dropped.
 * Call from loop() before checking MQTT state.
 */
void wifi_ensure_connected();

/**
 * Connect to the MQTT broker.
 * Publishes an online LWT message and calls subscribe_topics() for node-specific subs.
 *
 * @param client         PubSubClient instance
 * @param client_id      Unique MQTT client ID string
 * @param status_topic   Topic to publish {"online":true/false} to (retained)
 * @param subscribe_fn   Callback to subscribe to node-specific topics after connect
 */
void mqtt_connect(PubSubClient& client,
                  const char*   client_id,
                  const char*   status_topic,
                  void        (*subscribe_fn)());

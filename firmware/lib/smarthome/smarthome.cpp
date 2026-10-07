#include "smarthome.h"

void wifi_connect() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

void mqtt_connect(PubSubClient& client,
                  const char*   client_id,
                  const char*   status_topic,
                  void        (*subscribe_fn)()) {
    while (!client.connected()) {
        Serial.printf("Connecting to MQTT as %s...", client_id);
        if (client.connect(client_id,
                           nullptr, nullptr,
                           status_topic, 1, true,
                           "{\"online\":false}")) {
            Serial.println("connected");
            client.publish(status_topic, "{\"online\":true}", true);
            subscribe_fn();
        } else {
            Serial.printf("failed (rc=%d), retrying in 5s\n", client.state());
            delay(5000);
        }
    }
}

/**
 * SmarterHome — Entrance/Security ESP32 Node
 *
 * Features:
 *   - Relay-controlled electric door lock (GPIO 26)
 *   - PIR motion sensor (GPIO 13)
 *   - Doorbell push button (GPIO 32)
 *   - Status LED (GPIO 2)
 *   - Buzzer for local alerts (GPIO 25)
 *
 * MQTT Topics (subscribed):
 *   smarthome/entrance/door_lock/set    → {"locked":true|false}
 *
 * MQTT Topics (published):
 *   smarthome/entrance/motion/event     → {"detected":true,"timestamp":1234567890}
 *   smarthome/entrance/doorbell/event   → {"pressed":true,"timestamp":1234567890}
 *   smarthome/entrance/door_lock/status → {"locked":true}
 *   smarthome/entrance/status           → {"online":true}
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER     "192.168.1.100"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "esp32-entrance"

#define RELAY_LOCK_PIN  26
#define PIR_PIN         13
#define DOORBELL_PIN    32
#define LED_PIN         2
#define BUZZER_PIN      25

#define LOCK_ENGAGED   HIGH
#define LOCK_RELEASED  LOW

#define MOTION_COOLDOWN_MS    5000
#define DOORBELL_DEBOUNCE_MS  2000
#define AUTO_LOCK_DELAY_MS   10000

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

bool isLocked           = true;
unsigned long lastMotionTime   = 0;
unsigned long lastDoorbellTime = 0;
unsigned long unlockTime       = 0;
bool autoLockPending    = false;

void connectWiFi();
void connectMQTT();
void onMessage(char* topic, byte* payload, unsigned int length);
void setLock(bool locked);
void checkMotion();
void checkDoorbell();
void checkAutoLock();

void setLock(bool locked) {
    isLocked = locked;
    digitalWrite(RELAY_LOCK_PIN, locked ? LOCK_ENGAGED : LOCK_RELEASED);
    digitalWrite(LED_PIN, locked ? LOW : HIGH);

    JsonDocument doc;
    doc["locked"] = locked;
    char buf[32];
    serializeJson(doc, buf);
    mqtt.publish("smarthome/entrance/door_lock/status", buf, true);

    if (!locked) {
        unlockTime       = millis();
        autoLockPending  = true;
    } else {
        autoLockPending  = false;
    }
}

void onMessage(char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

    if (strcmp(topic, "smarthome/entrance/door_lock/set") == 0) {
        setLock(doc["locked"] | true);
    }
}

void checkMotion() {
    if (digitalRead(PIR_PIN) != HIGH) return;

    unsigned long now = millis();
    if (now - lastMotionTime < MOTION_COOLDOWN_MS) return;
    lastMotionTime = now;

    digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);

    JsonDocument doc;
    doc["detected"]  = true;
    doc["timestamp"] = now / 1000;
    char buf[64];
    serializeJson(doc, buf);
    mqtt.publish("smarthome/entrance/motion/event", buf);
}

void checkDoorbell() {
    if (digitalRead(DOORBELL_PIN) != LOW) return;

    unsigned long now = millis();
    if (now - lastDoorbellTime < DOORBELL_DEBOUNCE_MS) return;
    lastDoorbellTime = now;

    for (int i = 0; i < 2; i++) {
        digitalWrite(BUZZER_PIN, HIGH); delay(200);
        digitalWrite(BUZZER_PIN, LOW);  delay(150);
    }

    JsonDocument doc;
    doc["pressed"]   = true;
    doc["timestamp"] = now / 1000;
    char buf[64];
    serializeJson(doc, buf);
    mqtt.publish("smarthome/entrance/doorbell/event", buf);
}

void checkAutoLock() {
    if (autoLockPending && !isLocked && millis() - unlockTime > AUTO_LOCK_DELAY_MS) {
        setLock(true);
    }
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);
}

void connectMQTT() {
    while (!mqtt.connected()) {
        if (mqtt.connect(MQTT_CLIENT_ID, nullptr, nullptr,
                         "smarthome/entrance/status", 1, true,
                         "{\"online\":false}")) {
            mqtt.publish("smarthome/entrance/status", "{\"online\":true}", true);
            mqtt.subscribe("smarthome/entrance/door_lock/set");

            JsonDocument doc;
            doc["locked"] = isLocked;
            char buf[32];
            serializeJson(doc, buf);
            mqtt.publish("smarthome/entrance/door_lock/status", buf, true);
        } else {
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(RELAY_LOCK_PIN, OUTPUT);
    pinMode(LED_PIN,        OUTPUT);
    pinMode(BUZZER_PIN,     OUTPUT);
    pinMode(PIR_PIN,        INPUT);
    pinMode(DOORBELL_PIN,   INPUT_PULLUP);

    digitalWrite(BUZZER_PIN, LOW);
    setLock(true);

    connectWiFi();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(onMessage);
    connectMQTT();
}

void loop() {
    if (!mqtt.connected()) connectMQTT();
    mqtt.loop();
    checkMotion();
    checkDoorbell();
    checkAutoLock();
}

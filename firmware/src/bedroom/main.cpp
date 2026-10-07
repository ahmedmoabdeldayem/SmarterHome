/**
 * SmarterHome — Bedroom ESP32 Node
 *
 * Features:
 *   - 2-channel relay for lights (GPIO 26, 27)
 *   - IR blaster for AC control (GPIO 4)
 *   - DHT22 temperature & humidity sensor (GPIO 15)
 *
 * MQTT Topics (subscribed):
 *   smarthome/bedroom/light/{1-2}/set  → {"state":"on"|"off"}
 *   smarthome/bedroom/ac/set           → {"mode":"cool|heat|fan|off","temp":22}
 *
 * MQTT Topics (published):
 *   smarthome/bedroom/climate/status   → {"temp":23.5,"humidity":55.0}
 *   smarthome/bedroom/status           → {"online":true}
 */

#include <smarthome.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define MQTT_CLIENT_ID  "esp32-bedroom"

#define RELAY_1    26
#define RELAY_2    27
#define IR_TX_PIN  4
#define DHT_PIN    15
#define DHT_TYPE   DHT22

#define CLIMATE_REPORT_MS  10000

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
IRsend       irSend(IR_TX_PIN);
DHT          dht(DHT_PIN, DHT_TYPE);

unsigned long lastClimateReport = 0;
const int relayPins[2] = {RELAY_1, RELAY_2};

void onMessage(char* topic, byte* payload, unsigned int length);
void reportClimate();
void subscribeTopics();

void subscribeTopics() {
    mqtt.subscribe("smarthome/bedroom/light/+/set");
    mqtt.subscribe("smarthome/bedroom/ac/set");
}

void onMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

    for (int i = 1; i <= 2; i++) {
        if (topicStr == "smarthome/bedroom/light/" + String(i) + "/set") {
            digitalWrite(relayPins[i - 1], strcmp(doc["state"], "on") == 0 ? LOW : HIGH);
            return;
        }
    }

    if (topicStr == "smarthome/bedroom/ac/set") {
        const char* mode = doc["mode"] | "off";
        int temp = doc["temp"] | 24;
        IRCoolixAC ac(IR_TX_PIN);
        ac.begin();
        if (strcmp(mode, "off") == 0) {
            ac.off();
        } else {
            ac.on();
            ac.setTemp(temp);
            if      (strcmp(mode, "cool") == 0) ac.setMode(kCoolixCool);
            else if (strcmp(mode, "heat") == 0) ac.setMode(kCoolixHeat);
            else                                ac.setMode(kCoolixFan);
        }
        ac.send();
    }
}

void reportClimate() {
    float temp     = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temp) || isnan(humidity)) return;

    JsonDocument doc;
    doc["temp"]     = roundf(temp     * 10) / 10.0f;
    doc["humidity"] = roundf(humidity * 10) / 10.0f;
    char buf[64];
    serializeJson(doc, buf);
    mqtt.publish("smarthome/bedroom/climate/status", buf, true);
}

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < 2; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], HIGH);
    }
    irSend.begin();
    dht.begin();
    wifi_connect();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(onMessage);
    mqtt_connect(mqtt, MQTT_CLIENT_ID, "smarthome/bedroom/status", subscribeTopics);
}

void loop() {
    if (!mqtt.connected())
        mqtt_connect(mqtt, MQTT_CLIENT_ID, "smarthome/bedroom/status", subscribeTopics);
    mqtt.loop();
    if (millis() - lastClimateReport > CLIMATE_REPORT_MS) {
        lastClimateReport = millis();
        reportClimate();
    }
}

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
 *
 * Dependencies:
 *   - PubSubClient, IRremoteESP8266, ArduinoJson, DHT sensor library
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER     "192.168.1.100"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "esp32-bedroom"

#define RELAY_1     26
#define RELAY_2     27
#define IR_TX_PIN   4
#define DHT_PIN     15
#define DHT_TYPE    DHT22

#define CLIMATE_REPORT_MS  10000

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
IRsend irSend(IR_TX_PIN);
DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastClimateReport = 0;
const int relayPins[2] = {RELAY_1, RELAY_2};

void onMessage(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

  for (int i = 1; i <= 2; i++) {
    if (topicStr == "smarthome/bedroom/light/" + String(i) + "/set") {
      bool on = strcmp(doc["state"], "on") == 0;
      digitalWrite(relayPins[i - 1], on ? LOW : HIGH);
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
      if (strcmp(mode, "cool") == 0) ac.setMode(kCoolixCool);
      else if (strcmp(mode, "heat") == 0) ac.setMode(kCoolixHeat);
      else ac.setMode(kCoolixFan);
    }
    ac.send();
  }
}

void reportClimate() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (isnan(temp) || isnan(humidity)) return;

  StaticJsonDocument<64> doc;
  doc["temp"] = round(temp * 10) / 10.0;
  doc["humidity"] = round(humidity * 10) / 10.0;

  char buf[64];
  serializeJson(doc, buf);
  mqtt.publish("smarthome/bedroom/climate/status", buf, true);
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void connectMQTT() {
  while (!mqtt.connected()) {
    if (mqtt.connect(MQTT_CLIENT_ID, nullptr, nullptr,
                     "smarthome/bedroom/status", 1, true,
                     "{\"online\":false}")) {
      mqtt.publish("smarthome/bedroom/status", "{\"online\":true}", true);
      mqtt.subscribe("smarthome/bedroom/light/+/set");
      mqtt.subscribe("smarthome/bedroom/ac/set");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 2; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }
  irSend.begin();
  dht.begin();
  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
  if (millis() - lastClimateReport > CLIMATE_REPORT_MS) {
    lastClimateReport = millis();
    reportClimate();
  }
}

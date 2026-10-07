/**
 * SmarterHome — Living Room ESP32 Node
 *
 * Features:
 *   - 4-channel relay for lights/fans (GPIO 26, 27, 14, 12)
 *   - IR blaster for AC + TV control (GPIO 4)
 *   - ACS712 current sensor for energy monitoring (GPIO 34, analog)
 *
 * MQTT Topics (subscribed):
 *   smarthome/living_room/light/{1-4}/set   → {"state":"on"|"off"}
 *   smarthome/living_room/ac/set            → {"mode":"cool|heat|fan|off","temp":22}
 *   smarthome/living_room/tv/set            → {"cmd":"on|off|vol_up|vol_down|mute"}
 *
 * MQTT Topics (published):
 *   smarthome/living_room/energy/status     → {"watts":350.5,"amps":1.59}
 *   smarthome/living_room/status            → {"online":true}
 *
 * Dependencies (install via Arduino Library Manager):
 *   - PubSubClient by Nick O'Leary
 *   - IRremoteESP8266 by David Conran et al.
 *   - ArduinoJson by Benoit Blanchon
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>   // Change to match your AC brand
#include <ArduinoJson.h>

// ── Configuration ──────────────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER     "192.168.1.100"   // Raspberry Pi local IP
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "esp32-living-room"

// Relay pins (active LOW for most relay boards)
#define RELAY_1  26
#define RELAY_2  27
#define RELAY_3  14
#define RELAY_4  12

// IR transmitter pin
#define IR_TX_PIN  4

// ACS712 analog pin (30A variant: sensitivity = 66mV/A)
#define CURRENT_SENSOR_PIN  34
#define ACS712_SENSITIVITY  0.066f  // V/A for 30A module
#define ACS712_OFFSET       2.5f    // Voltage at 0A (half of 5V)
#define ADC_REF_VOLTAGE     3.3f
#define ADC_RESOLUTION      4096.0f

// Energy report interval (ms)
#define ENERGY_REPORT_MS  5000

// ── Globals ─────────────────────────────────────────────────────
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
IRsend irSend(IR_TX_PIN);

unsigned long lastEnergyReport = 0;
bool relayState[4] = {false, false, false, false};
const int relayPins[4] = {RELAY_1, RELAY_2, RELAY_3, RELAY_4};

// ── MQTT Callback ────────────────────────────────────────────────
void onMessage(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  StaticJsonDocument<128> doc;

  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

  // Light control: smarthome/living_room/light/{1-4}/set
  for (int i = 1; i <= 4; i++) {
    String lightTopic = "smarthome/living_room/light/" + String(i) + "/set";
    if (topicStr == lightTopic) {
      bool on = strcmp(doc["state"], "on") == 0;
      relayState[i - 1] = on;
      digitalWrite(relayPins[i - 1], on ? LOW : HIGH);  // Active LOW relay
      return;
    }
  }

  // AC control
  if (topicStr == "smarthome/living_room/ac/set") {
    const char* mode = doc["mode"] | "off";
    int temp = doc["temp"] | 24;
    sendACCommand(mode, temp);
    return;
  }

  // TV control
  if (topicStr == "smarthome/living_room/tv/set") {
    const char* cmd = doc["cmd"] | "";
    sendTVCommand(cmd);
  }
}

// ── IR Commands ──────────────────────────────────────────────────
void sendACCommand(const char* mode, int temp) {
  // Example using Coolix protocol — replace with your AC's protocol
  IRCoolixAC ac(IR_TX_PIN);
  ac.begin();
  if (strcmp(mode, "off") == 0) {
    ac.off();
  } else {
    ac.on();
    ac.setTemp(temp);
    if (strcmp(mode, "cool") == 0) ac.setMode(kCoolixCool);
    else if (strcmp(mode, "heat") == 0) ac.setMode(kCoolixHeat);
    else if (strcmp(mode, "fan") == 0) ac.setMode(kCoolixFan);
  }
  ac.send();
}

void sendTVCommand(const char* cmd) {
  // Example Samsung TV NEC codes — replace with your TV's codes
  if (strcmp(cmd, "on") == 0 || strcmp(cmd, "off") == 0)
    irSend.sendNEC(0xE0E040BF, 32);  // Power toggle
  else if (strcmp(cmd, "vol_up") == 0)
    irSend.sendNEC(0xE0E0E01F, 32);
  else if (strcmp(cmd, "vol_down") == 0)
    irSend.sendNEC(0xE0E0D02F, 32);
  else if (strcmp(cmd, "mute") == 0)
    irSend.sendNEC(0xE0E0F00F, 32);
}

// ── Energy Monitoring ─────────────────────────────────────────────
float readCurrentAmps() {
  // Average 100 samples for stability
  long sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += analogRead(CURRENT_SENSOR_PIN);
    delayMicroseconds(100);
  }
  float avgADC = sum / 100.0f;
  float voltage = (avgADC / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
  float amps = (voltage - ACS712_OFFSET) / ACS712_SENSITIVITY;
  return abs(amps);
}

void reportEnergy() {
  float amps = readCurrentAmps();
  float watts = amps * 220.0f;  // Adjust to your mains voltage

  StaticJsonDocument<64> doc;
  doc["amps"] = round(amps * 100) / 100.0;
  doc["watts"] = round(watts * 10) / 10.0;

  char buf[64];
  serializeJson(doc, buf);
  mqtt.publish("smarthome/living_room/energy/status", buf);
}

// ── WiFi & MQTT ──────────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect(MQTT_CLIENT_ID, nullptr, nullptr,
                     "smarthome/living_room/status", 1, true,
                     "{\"online\":false}")) {
      Serial.println("connected");
      mqtt.publish("smarthome/living_room/status", "{\"online\":true}", true);

      // Subscribe to control topics
      mqtt.subscribe("smarthome/living_room/light/+/set");
      mqtt.subscribe("smarthome/living_room/ac/set");
      mqtt.subscribe("smarthome/living_room/tv/set");
    } else {
      Serial.printf("failed (rc=%d), retrying in 5s\n", mqtt.state());
      delay(5000);
    }
  }
}

// ── Arduino Setup & Loop ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Relays off by default (active LOW)
  }

  irSend.begin();
  connectWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  connectMQTT();
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  if (millis() - lastEnergyReport > ENERGY_REPORT_MS) {
    lastEnergyReport = millis();
    reportEnergy();
  }
}

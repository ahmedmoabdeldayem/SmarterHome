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
 */

#include <smarthome.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>
#include <ArduinoJson.h>

#define MQTT_CLIENT_ID  "esp32-living-room"

#define RELAY_1  26
#define RELAY_2  27
#define RELAY_3  14
#define RELAY_4  12

#define IR_TX_PIN  4

#define CURRENT_SENSOR_PIN  34
#define ACS712_SENSITIVITY  0.066f
#define ACS712_OFFSET       2.5f
#define ADC_REF_VOLTAGE     3.3f
#define ADC_RESOLUTION      4096.0f

#define ENERGY_REPORT_MS  5000

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
// Two globals on the same pin: each claims its own ESP32 RMT channel (the GPIO
// is shared). Both are initialised once in setup() to avoid per-call begin()
// exhausting the 8 available RMT channels.
IRCoolixAC   acSend(IR_TX_PIN);
IRsend       irSend(IR_TX_PIN);

unsigned long lastEnergyReport = 0;
const int relayPins[4] = {RELAY_1, RELAY_2, RELAY_3, RELAY_4};

void onMessage(char* topic, byte* payload, unsigned int length);
void sendACCommand(const char* mode, int temp);
void sendTVCommand(const char* cmd);
float readCurrentAmps();
void reportEnergy();
void subscribeTopics();

void subscribeTopics() {
    mqtt.subscribe("smarthome/living_room/light/+/set");
    mqtt.subscribe("smarthome/living_room/ac/set");
    mqtt.subscribe("smarthome/living_room/tv/set");
}

void onMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

    for (int i = 1; i <= 4; i++) {
        if (topicStr == "smarthome/living_room/light/" + String(i) + "/set") {
            const char* state = doc["state"] | "off";
            bool on = strcmp(state, "on") == 0;
            digitalWrite(relayPins[i - 1], on ? LOW : HIGH);

            JsonDocument statusDoc;
            statusDoc["state"] = on ? "on" : "off";
            char statusBuf[32];
            serializeJson(statusDoc, statusBuf);
            mqtt.publish(("smarthome/living_room/light/" + String(i) + "/status").c_str(),
                         statusBuf, true);
            return;
        }
    }
    if (topicStr == "smarthome/living_room/ac/set") {
        sendACCommand(doc["mode"] | "off", doc["temp"] | 24);
    } else if (topicStr == "smarthome/living_room/tv/set") {
        sendTVCommand(doc["cmd"] | "");
    }
}

void sendACCommand(const char* mode, int temp) {
    if (strcmp(mode, "off") == 0) {
        acSend.off();
    } else {
        acSend.on();
        acSend.setTemp(temp);
        if      (strcmp(mode, "cool") == 0) acSend.setMode(kCoolixCool);
        else if (strcmp(mode, "heat") == 0) acSend.setMode(kCoolixHeat);
        else                                acSend.setMode(kCoolixFan);
    }
    acSend.send();
}

void sendTVCommand(const char* cmd) {
    if      (strcmp(cmd, "on")       == 0 ||
             strcmp(cmd, "off")      == 0) irSend.sendNEC(0xE0E040BF, 32);
    else if (strcmp(cmd, "vol_up")   == 0) irSend.sendNEC(0xE0E0E01F, 32);
    else if (strcmp(cmd, "vol_down") == 0) irSend.sendNEC(0xE0E0D02F, 32);
    else if (strcmp(cmd, "mute")     == 0) irSend.sendNEC(0xE0E0F00F, 32);
}

float readCurrentAmps() {
    // Sample over one full 50 Hz cycle (20 ms) and compute RMS for AC loads.
    const int N = 200;
    float sumSq = 0.0f;
    for (int i = 0; i < N; i++) {
        float voltage = (analogRead(CURRENT_SENSOR_PIN) / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
        float amps = (voltage - ACS712_OFFSET) / ACS712_SENSITIVITY;
        sumSq += amps * amps;
        delayMicroseconds(100);
    }
    return sqrtf(sumSq / N);
}

void reportEnergy() {
    float amps  = readCurrentAmps();
    float watts = amps * MAINS_VOLTAGE;
    JsonDocument doc;
    doc["amps"]  = roundf(amps  * 100) / 100.0f;
    doc["watts"] = roundf(watts * 10)  / 10.0f;
    char buf[64];
    serializeJson(doc, buf);
    mqtt.publish("smarthome/living_room/energy/status", buf);
}

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < 4; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], HIGH);
    }
    // 11 dB attenuation required for ACS712: its zero-current output is 2.5 V,
    // which exceeds the default 0 dB ceiling (~1.1 V) and would saturate the ADC.
    analogSetPinAttenuation(CURRENT_SENSOR_PIN, ADC_11db);
    acSend.begin();
    irSend.begin();
    wifi_connect();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(onMessage);
    mqtt_connect(mqtt, MQTT_CLIENT_ID, "smarthome/living_room/status", subscribeTopics);

    // Publish initial relay states as retained so the dashboard shows the correct
    // state immediately after a power cycle (all relays start HIGH = off).
    for (int i = 1; i <= 4; i++) {
        String statusTopic = "smarthome/living_room/light/" + String(i) + "/status";
        mqtt.publish(statusTopic.c_str(), "{\"state\":\"off\"}", true);
    }
}

void loop() {
    wifi_ensure_connected();
    if (!mqtt.connected())
        mqtt_connect(mqtt, MQTT_CLIENT_ID, "smarthome/living_room/status", subscribeTopics);
    mqtt.loop();
    if (millis() - lastEnergyReport > ENERGY_REPORT_MS) {
        lastEnergyReport = millis();
        reportEnergy();
    }
}

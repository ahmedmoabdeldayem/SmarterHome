/**
 * SmarterHome — Kitchen ESP32 Node
 *
 * Features:
 *   - 1-channel relay for light (GPIO 26)
 *   - MQ-2 gas/smoke sensor (GPIO 34, analog)
 *   - ACS712 current sensor for energy monitoring (GPIO 35, analog)
 *   - Buzzer for local gas alert (GPIO 25)
 *
 * MQTT Topics (subscribed):
 *   smarthome/kitchen/light/set        → {"state":"on"|"off"}
 *
 * MQTT Topics (published):
 *   smarthome/kitchen/gas/status       → {"ppm":120,"alert":false}
 *   smarthome/kitchen/energy/status    → {"watts":800.0,"amps":3.6}
 *   smarthome/kitchen/status           → {"online":true}
 */

#include <smarthome.h>
#include <ArduinoJson.h>

#define MQTT_CLIENT_ID  "esp32-kitchen"

#define RELAY_LIGHT_PIN    26
#define GAS_SENSOR_PIN     34
#define CURRENT_SENSOR_PIN 35
#define BUZZER_PIN         25

#define ACS712_SENSITIVITY  0.066f
#define ACS712_OFFSET       2.5f
#define ADC_REF_VOLTAGE     3.3f
#define ADC_RESOLUTION      4096.0f

#define SENSOR_REPORT_MS  3000

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastReport   = 0;
bool          gasAlertActive = false;

void onMessage(char* topic, byte* payload, unsigned int length);
int   readGasPPM();
float readCurrentAmps();
void  reportSensors();
void  subscribeTopics();

void subscribeTopics() {
    mqtt.subscribe("smarthome/kitchen/light/set");
}

void onMessage(char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;
    if (strcmp(topic, "smarthome/kitchen/light/set") == 0)
        digitalWrite(RELAY_LIGHT_PIN, strcmp(doc["state"], "on") == 0 ? LOW : HIGH);
}

int readGasPPM() {
    return map(analogRead(GAS_SENSOR_PIN), 0, 4095, 0, 10000);
}

float readCurrentAmps() {
    long sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += analogRead(CURRENT_SENSOR_PIN);
        delayMicroseconds(100);
    }
    float voltage = (sum / 100.0f / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
    return fabsf((voltage - ACS712_OFFSET) / ACS712_SENSITIVITY);
}

void reportSensors() {
    int  ppm   = readGasPPM();
    bool alert = ppm > 1000;

    if (alert && !gasAlertActive)  { gasAlertActive = true;  digitalWrite(BUZZER_PIN, HIGH); }
    if (!alert && gasAlertActive)  { gasAlertActive = false; digitalWrite(BUZZER_PIN, LOW);  }

    {
        JsonDocument doc;
        doc["ppm"] = ppm; doc["alert"] = alert;
        char buf[64]; serializeJson(doc, buf);
        mqtt.publish("smarthome/kitchen/gas/status", buf);
    }
    {
        float amps = readCurrentAmps(), watts = amps * 220.0f;
        JsonDocument doc;
        doc["amps"]  = roundf(amps  * 100) / 100.0f;
        doc["watts"] = roundf(watts * 10)  / 10.0f;
        char buf[64]; serializeJson(doc, buf);
        mqtt.publish("smarthome/kitchen/energy/status", buf);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_LIGHT_PIN,    OUTPUT); digitalWrite(RELAY_LIGHT_PIN, HIGH);
    pinMode(BUZZER_PIN,         OUTPUT); digitalWrite(BUZZER_PIN,      LOW);
    pinMode(GAS_SENSOR_PIN,     INPUT);
    pinMode(CURRENT_SENSOR_PIN, INPUT);
    wifi_connect();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(onMessage);
    mqtt_connect(mqtt, MQTT_CLIENT_ID, "smarthome/kitchen/status", subscribeTopics);
}

void loop() {
    if (!mqtt.connected())
        mqtt_connect(mqtt, MQTT_CLIENT_ID, "smarthome/kitchen/status", subscribeTopics);
    mqtt.loop();
    if (millis() - lastReport > SENSOR_REPORT_MS) {
        lastReport = millis();
        reportSensors();
    }
}

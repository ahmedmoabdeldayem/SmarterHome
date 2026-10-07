# SmarterHome — Setup Guide

## Prerequisites

- AWS account with IAM credentials configured (`aws configure`)
- Python 3.9+ on your dev machine and Raspberry Pi
- Arduino IDE with ESP32 board support installed
- Flutter SDK 3.x

---

## Step 1 — AWS IoT Core Provisioning

```bash
cd aws/setup
pip install boto3
python provision.py
```

This creates:
- 5 IoT Things (hub + 4 rooms)
- X.509 certificates saved to `aws/certs/<thing-name>/`
- A shared `SmarterHomePolicy`
- `aws/certs/endpoint.txt` with your IoT endpoint

**Copy hub certs to RPi:**
```bash
scp -r aws/certs/smarthome-hub/ pi@192.168.1.100:~/SmarterHome/hub/certs/
scp aws/certs/AmazonRootCA1.pem pi@192.168.1.100:~/SmarterHome/hub/certs/
scp aws/certs/endpoint.txt pi@192.168.1.100:~/SmarterHome/aws/certs/
```

---

## Step 2 — Raspberry Pi Hub

**Install dependencies:**
```bash
sudo apt install -y mosquitto mosquitto-clients python3-pip cmake build-essential libssl-dev git
sudo cp hub/config/mosquitto.conf /etc/mosquitto/conf.d/smarthome.conf
sudo systemctl restart mosquitto

pip3 install -r hub/config/requirements.txt
```

**Build the C++ bridge:**
```bash
cd hub/bridge
cmake -B build && cmake --build build -j$(nproc)
# Binary will be at: hub/bridge/build/smarthome_bridge
cd ../..
```

**Run bridge and automation:**
```bash
# Test manually first
./hub/bridge/build/smarthome_bridge
python3 hub/automation.py

# Then install as systemd services
sudo cp hub/config/smarthome-bridge.service /etc/systemd/system/
sudo cp hub/config/smarthome-automation.service /etc/systemd/system/
sudo systemctl enable smarthome-bridge smarthome-automation
sudo systemctl start smarthome-bridge smarthome-automation
```

**Find your RPi's IP address:**
```bash
hostname -I
```
→ Update `MQTT_BROKER` in all ESP32 sketches to this IP.

---

## Step 3 — ESP32 Firmware

For each ESP32:

1. Open `firmware/<room>/<room>.ino` in Arduino IDE
2. Fill in:
   - `WIFI_SSID` and `WIFI_PASSWORD`
   - `MQTT_BROKER` = RPi's IP address
3. Select Board: `ESP32 Dev Module`
4. Flash via USB

**Required Arduino libraries** (install via Library Manager):
- `PubSubClient` by Nick O'Leary
- `IRremoteESP8266` by David Conran
- `ArduinoJson` by Benoit Blanchon
- `DHT sensor library` by Adafruit (bedroom only)

---

## Step 4 — AWS Cognito (for mobile app auth)

In AWS Console → Cognito → Create User Pool:
1. Sign-in: Email + password
2. Note your **User Pool ID** and **App Client ID**
3. Update `lib/services/auth_service.dart`:
   - `_userPoolId`
   - `_clientId`

For IoT WebSocket access from mobile, set up an **AWS IoT Custom Authorizer** or use **Cognito Identity Pools** to vend IoT credentials. See the [AWS IoT custom auth docs](https://docs.aws.amazon.com/iot/latest/developerguide/custom-authentication.html).

---

## Step 5 — Mobile App

```bash
cd mobile_app/smarter_home
flutter pub get
flutter run
```

For release builds:
```bash
flutter build apk --release      # Android
flutter build ios --release      # iOS (requires Xcode)
```

---

## Wiring Reference

### Living Room ESP32
| Pin | Component |
|-----|-----------|
| GPIO 26 | Relay 1 (Light 1) |
| GPIO 27 | Relay 2 (Light 2) |
| GPIO 14 | Relay 3 (Light 3 / Fan) |
| GPIO 12 | Relay 4 (Light 4) |
| GPIO 4  | IR LED (AC/TV) |
| GPIO 34 | ACS712 OUT (energy sensor) |
| 3.3V / GND | Power |

### Bedroom ESP32
| Pin | Component |
|-----|-----------|
| GPIO 26 | Relay 1 (Light 1) |
| GPIO 27 | Relay 2 (Light 2) |
| GPIO 4  | IR LED (AC) |
| GPIO 15 | DHT22 DATA |

### Kitchen ESP32
| Pin | Component |
|-----|-----------|
| GPIO 26 | Relay (Light) |
| GPIO 34 | MQ-2 AOUT (gas sensor) |
| GPIO 35 | ACS712 OUT (energy sensor) |
| GPIO 25 | Buzzer |

### Entrance ESP32
| Pin | Component |
|-----|-----------|
| GPIO 26 | Relay (Door lock) |
| GPIO 13 | PIR OUT (motion sensor) |
| GPIO 32 | Doorbell button (pull-up) |
| GPIO 2  | Status LED |
| GPIO 25 | Buzzer |

---

## Verifying the System

1. **Mosquitto broker:** `mosquitto_sub -h localhost -t 'smarthome/#' -v`
2. **ESP32 connected:** Should see `smarthome/<room>/status {"online":true}`
3. **Bridge working:** In AWS Console → IoT Core → MQTT test client, subscribe to `smarthome/#`
4. **Mobile app:** Toggle a light, confirm relay clicks
5. **Automation:** Walk past PIR sensor, confirm living room light turns on

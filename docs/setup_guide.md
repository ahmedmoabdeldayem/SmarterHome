# SmarterHome — Setup Guide

## Prerequisites

- Python 3.9+ on your dev machine (for future AWS setup only)
- cmake, build-essential, libssl-dev on Raspberry Pi
- Arduino IDE or PlatformIO on your laptop for ESP32 flashing
- Flutter SDK (if adding remote access later)

---

## Step 1 — Raspberry Pi OS

Flash Raspberry Pi OS Lite (64-bit) onto a microSD card.
See `docs/compile_and_flash.md` → Section 1 for full instructions.

Once booted, SSH in:
```bash
ssh pi@smarthome-hub.local
```

---

## Step 2 — Raspberry Pi Hub

**Install dependencies:**
```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients cmake build-essential git
```

**Configure Mosquitto:**
```bash
sudo cp ~/SmarterHome/hub/config/mosquitto.conf /etc/mosquitto/conf.d/smarthome.conf
sudo systemctl restart mosquitto
```

**Build the automation engine:**
```bash
cd ~/SmarterHome/hub
cmake -B build && cmake --build build -j$(nproc)
```

**Install as systemd service (auto-start on boot):**
```bash
sudo cp ~/SmarterHome/hub/config/smarthome-automation.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable smarthome-automation
sudo systemctl start smarthome-automation
```

**Find your RPi's IP address:**
```bash
hostname -I
```
→ Note this IP — you'll need it for the ESP32 firmware and MQTT Dash.

---

## Step 3 — ESP32 Firmware

1. Open `firmware/lib/smarthome/smarthome.h` and fill in:
```cpp
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER    "192.168.1.100"   // ← your RPi's IP
```

2. Flash each ESP32 via USB:
```bash
cd firmware
pio run -e living_room --target upload
pio run -e bedroom     --target upload
pio run -e kitchen     --target upload
pio run -e entrance    --target upload
```

See `docs/compile_and_flash.md` → Section 2 for full details.

---

## Step 4 — MQTT Dash (Android App)

1. Install **MQTT Dash** from the Play Store
2. Open the app → Menu → **Import**
3. Import `mqtt_dash/smarthome_dashboard.json`
4. Edit the connection and set the broker IP to your RPi's IP address
5. Connect — all your devices should appear

---

## Verifying the System

**Check ESP32s are online:**
```bash
mosquitto_sub -h localhost -t 'smarthome/#' -v
# Should see: smarthome/<room>/status {"online":true}
```

**Check automation engine is running:**
```bash
sudo systemctl status smarthome-automation
journalctl -u smarthome-automation -f
```

**End-to-end test:**
1. Toggle a light in MQTT Dash → relay should click
2. Walk past PIR sensor → living room light turns on automatically
3. Press doorbell button → MQTT Dash shows alert

---

## Adding AWS Remote Access Later

When you're ready to add remote access:
1. Set up AWS IoT Core (add `aws/` setup scripts back)
2. Re-add the C++ bridge (`hub/bridge/`)
3. Or add an AWS IoT Custom Authorizer to use MQTT Dash remotely

Everything is designed to be extended — the MQTT topic structure stays the same.

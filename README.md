# SmarterHome

A local-first smart home system built with ESP32 microcontrollers, a Raspberry Pi hub, and MQTT.
No cloud required — the home keeps working during internet outages.

```
Phone (MQTT Dash)
      │
      │ Wi-Fi (MQTT)
      ▼
Raspberry Pi Hub
  ├── Mosquitto (MQTT broker)
  └── smarthome_automation (C++ rules engine)
      │
      │ Wi-Fi (MQTT)
      ▼
ESP32 nodes (one per room)
  ├── Living Room — lights, IR blaster, energy monitor
  ├── Bedroom     — lights, IR blaster, temp/humidity sensor
  ├── Kitchen     — light, gas sensor, energy monitor
  └── Entrance    — door lock, PIR motion, doorbell
```

## Quick Start

### 1. Set up credentials

```bash
cp firmware/lib/smarthome/secrets.h.template firmware/lib/smarthome/secrets.h
# Edit secrets.h with your Wi-Fi SSID, password, RPi IP, and MQTT credentials
```

### 2. Flash ESP32 firmware

```bash
cd firmware
pio run -e living_room --target upload
pio run -e bedroom     --target upload
pio run -e kitchen     --target upload
pio run -e entrance    --target upload
```

### 3. Set up the Raspberry Pi hub

```bash
# Install Mosquitto
sudo apt-get install -y mosquitto mosquitto-clients

# Create MQTT credentials (use the same password as in secrets.h)
sudo mosquitto_passwd -c /etc/mosquitto/passwd smarthome

# Install broker config
sudo cp hub/config/mosquitto.conf /etc/mosquitto/conf.d/smarthome.conf
sudo systemctl restart mosquitto

# Build and install the automation engine
cd hub && cmake -B build && cmake --build build -j$(nproc)
sudo cmake --install build

# Enable as a systemd service
sudo cp hub/config/smarthome-automation.service /etc/systemd/system/
sudo systemctl enable --now smarthome-automation
```

### 4. Connect MQTT Dash

Import `mqtt_dash/smarthome_dashboard.json` into the [MQTT Dash](https://play.google.com/store/apps/details?id=net.routix.mqttdash) app and point it at your RPi's IP.

## Automation Rules

Rules are defined in `hub/automation/src/rules.h` — add or remove entries there, no other changes needed.

| Trigger | Condition | Action |
|---|---|---|
| Entrance motion | `detected: true` | Turn on living room light |
| Kitchen gas | `alert: true` | Broadcast gas alert |
| Bedroom climate | `temp > 28°C` | Start AC at 22°C |
| Doorbell | `pressed: true` | Publish doorbell alert |

## Docs

- [Project Explainer](docs/project_explainer.md) — architecture, hardware, full flow
- [Setup Guide](docs/setup_guide.md) — step-by-step deployment
- [Compile & Flash Guide](docs/compile_and_flash.md) — firmware and hub build instructions

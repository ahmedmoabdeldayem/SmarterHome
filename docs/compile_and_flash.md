# SmarterHome — Compile & Flash Guide

This file explains how to compile every piece of software in this project
and how to flash/deploy it onto the correct hardware.

---

## Overview

| Component | Language | Tool | Target Hardware |
|-----------|----------|------|-----------------|
| Living Room firmware | C++ | PlatformIO | ESP32 (living room) |
| Bedroom firmware | C++ | PlatformIO | ESP32 (bedroom) |
| Kitchen firmware | C++ | PlatformIO | ESP32 (kitchen) |
| Entrance firmware | C++ | PlatformIO | ESP32 (entrance) |
| Automation Engine | C++ | CMake | Raspberry Pi 4 |
| Raspberry Pi OS | — | RPi Imager | Raspberry Pi 4 (microSD) |
| Mobile Dashboard | — | MQTT Dash (Play Store) | Android phone |

---

## 1. Raspberry Pi OS

### What it is
The operating system that runs on the Raspberry Pi microSD card.
Everything else on the RPi runs on top of this.

### How to flash

**Step 1 — Download Raspberry Pi Imager**

Download from `raspberrypi.com/software` and install on your laptop.

**Step 2 — Flash**
1. Insert microSD card (16GB+) into your laptop
2. Open Raspberry Pi Imager
3. Click **Choose OS** → **Raspberry Pi OS Lite (64-bit)**
4. Click **Choose Storage** → select your microSD card
5. Click the **gear icon (⚙)** and configure:
   - Hostname: `smarthome-hub`
   - Enable SSH: yes
   - Username: `pi`
   - Password: *(choose a strong password)*
   - Wi-Fi SSID and password: *(your home network)*
6. Click **Write** and wait for it to finish
7. Insert microSD into the Raspberry Pi and power on

**Step 3 — Verify**
```bash
ssh pi@smarthome-hub.local
# You should get a shell prompt on the RPi
```

---

## 2. ESP32 Firmware

All 4 ESP32 nodes are compiled from a single PlatformIO project at `firmware/`.

### Prerequisites — install once on your laptop

```bash
pip install platformio
```

Or install the **PlatformIO IDE** extension in VSCode.

### Before compiling — fill in your settings

Open `firmware/lib/smarthome/smarthome.h` and set:
```cpp
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER    "192.168.1.100"    // your Raspberry Pi's IP address
```

To find your RPi's IP:
```bash
ssh pi@smarthome-hub.local "hostname -I"
```

### Compile all 4 nodes at once

```bash
cd firmware
pio run
```

Binary images produced at:
```
firmware/.pio/build/living_room/firmware.bin
firmware/.pio/build/bedroom/firmware.bin
firmware/.pio/build/kitchen/firmware.bin
firmware/.pio/build/entrance/firmware.bin
```

### Flash each ESP32

Connect each ESP32 to your laptop via USB, one at a time.

```bash
cd firmware
pio run -e living_room --target upload    # Living Room ESP32
pio run -e bedroom     --target upload    # Bedroom ESP32
pio run -e kitchen     --target upload    # Kitchen ESP32
pio run -e entrance    --target upload    # Entrance ESP32
```

### Watch serial output (debugging)

```bash
pio device monitor -e living_room    # replace with the room you're debugging
```

You should see `WiFi connected` and `connected to MQTT`.

### Other useful commands

```bash
pio run --target clean                    # delete build artifacts
pio run -e bedroom                        # compile one node without flashing
pio run -e kitchen --target upload && pio device monitor -e kitchen
```

---

## 3. Raspberry Pi Hub — Automation Engine

The automation engine is a C++ program built with CMake.
Run these commands **on the Raspberry Pi** (SSH in first).

### Prerequisites — install once on the RPi

```bash
sudo apt update
sudo apt install -y cmake build-essential git
```

### Copy the project to the RPi

From your laptop:
```bash
scp -r /path/to/SmarterHome pi@smarthome-hub.local:~/SmarterHome
```

### Compile

```bash
cd ~/SmarterHome/hub
cmake -B build
cmake --build build -j$(nproc)
```

First build takes a few minutes (downloads and compiles Paho MQTT C++).
Subsequent builds are fast.

Binary at:
```
hub/build/automation/smarthome_automation
```

### Run manually (for testing)

```bash
./hub/build/automation/smarthome_automation
```

You should see `Connected to local Mosquitto broker` and a list of subscribed topics.

### Install as systemd service (auto-start on boot)

```bash
sudo cp ~/SmarterHome/hub/config/smarthome-automation.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable smarthome-automation
sudo systemctl start smarthome-automation
```

**Useful service commands:**
```bash
sudo systemctl status smarthome-automation      # check if running
journalctl -u smarthome-automation -f           # live logs
sudo systemctl restart smarthome-automation     # restart after recompile
```

---

## 4. MQTT Dash (Android)

No compilation needed — download from the Play Store.

### Setup
1. Install **MQTT Dash** from Play Store
2. Open app → tap Menu (⋮) → **Import**
3. Import `mqtt_dash/smarthome_dashboard.json` from this repo
4. Tap the connection → set **Host** to your RPi's IP address
5. Tap **Connect**

All rooms and devices will appear as tiles. Tap a switch to control a device.

### Manual setup (if import doesn't work)
Create a new dashboard and add tiles manually:

| Device | Type | Publish topic | Subscribe topic | Payload ON | Payload OFF |
|--------|------|--------------|-----------------|-----------|------------|
| Living Room Light 1 | Switch | `smarthome/living_room/light/1/set` | `.../status` | `{"state":"on"}` | `{"state":"off"}` |
| Living Room Light 2 | Switch | `smarthome/living_room/light/2/set` | `.../status` | `{"state":"on"}` | `{"state":"off"}` |
| Living Room Light 3 | Switch | `smarthome/living_room/light/3/set` | `.../status` | `{"state":"on"}` | `{"state":"off"}` |
| Living Room Light 4 | Switch | `smarthome/living_room/light/4/set` | `.../status` | `{"state":"on"}` | `{"state":"off"}` |
| Bedroom Light 1 | Switch | `smarthome/bedroom/light/1/set` | `.../status` | `{"state":"on"}` | `{"state":"off"}` |
| Bedroom Light 2 | Switch | `smarthome/bedroom/light/2/set` | `.../status` | `{"state":"on"}` | `{"state":"off"}` |
| Kitchen Light | Switch | `smarthome/kitchen/light/set` | `.../status` | `{"state":"on"}` | `{"state":"off"}` |
| Door Lock | Switch | `smarthome/entrance/door_lock/set` | `.../status` | `{"locked":false}` | `{"locked":true}` |
| Bedroom Temp | Text | — | `smarthome/bedroom/climate/status` | — | — |
| Kitchen Gas | Text | — | `smarthome/kitchen/gas/status` | — | — |
| Motion | Text | — | `smarthome/entrance/motion/event` | — | — |

---

## Full Deployment Checklist

Follow this order exactly.

- [ ] **1.** Flash Raspberry Pi OS onto microSD and boot the RPi
- [ ] **2.** SSH into RPi and note its IP address (`hostname -I`)
- [ ] **3.** Install Mosquitto on RPi and copy `mosquitto.conf`
- [ ] **4.** Fill in `firmware/lib/smarthome/smarthome.h` with Wi-Fi credentials and RPi IP
- [ ] **5.** Build automation engine on RPi (`cmake -B build && cmake --build build`)
- [ ] **6.** Install and start automation systemd service
- [ ] **7.** Flash each ESP32 via USB (`pio run -e <room> --target upload`)
- [ ] **8.** Verify ESP32s appear online: `mosquitto_sub -h localhost -t 'smarthome/#' -v`
- [ ] **9.** Set up MQTT Dash — import config, set RPi IP, connect
- [ ] **10.** Test end-to-end: toggle a light in MQTT Dash → relay clicks

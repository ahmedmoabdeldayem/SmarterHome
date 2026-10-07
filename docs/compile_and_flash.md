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
| MQTT Bridge | C++ | CMake | Raspberry Pi 4 |
| Automation Engine | C++ | CMake | Raspberry Pi 4 |
| Raspberry Pi OS | — | RPi Imager | Raspberry Pi 4 (microSD) |
| Mobile App | Dart/Flutter | Flutter SDK | Android / iOS phone |

---

## 1. Raspberry Pi OS

### What it is
The operating system that runs on the Raspberry Pi microSD card.
Everything else on the RPi runs on top of this.

### How to flash

**Step 1 — Download the image**
```bash
chmod +x images/download_rpi_os.sh
./images/download_rpi_os.sh
# Image saved to: images/rpi_os/raspios-bookworm-arm64-lite.img.xz
```

**Step 2 — Flash with Raspberry Pi Imager**
1. Download Raspberry Pi Imager from `raspberrypi.com/software`
2. Open Imager
3. Click **Choose OS** → **Use custom** → select `images/rpi_os/raspios-bookworm-arm64-lite.img.xz`
4. Click **Choose Storage** → select your microSD card (16GB+)
5. Click the **gear icon (⚙)** and configure:
   - Hostname: `smarthome-hub`
   - Enable SSH: yes
   - Username: `pi`
   - Password: *(choose a strong password)*
   - Wi-Fi SSID and password: *(your home network)*
6. Click **Write** and wait for it to finish
7. Insert the microSD into the Raspberry Pi and power it on

**Step 3 — Verify it booted**
```bash
ssh pi@smarthome-hub.local
# You should get a shell prompt on the RPi
```

---

## 2. ESP32 Firmware

All 4 ESP32 nodes are compiled from a single PlatformIO project located at `firmware/`.

### Prerequisites — install once on your laptop

```bash
pip install platformio
```

Or install the **PlatformIO IDE extension** in VSCode for a GUI experience.

### Before compiling — fill in your settings

Open each `firmware/src/<room>/main.cpp` and set:
```cpp
#define WIFI_SSID      "YOUR_WIFI_SSID"      // your home Wi-Fi name
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"   // your home Wi-Fi password
```

Also set the `MQTT_BROKER` in `firmware/lib/smarthome/smarthome.h`:
```cpp
#define MQTT_BROKER    "192.168.1.100"        // your Raspberry Pi's IP address
```

To find the RPi's IP address:
```bash
ssh pi@smarthome-hub.local "hostname -I"
```

### Compile all 4 nodes at once

```bash
cd firmware
pio run
```

This produces 4 binary images at:
```
firmware/.pio/build/living_room/firmware.bin
firmware/.pio/build/bedroom/firmware.bin
firmware/.pio/build/kitchen/firmware.bin
firmware/.pio/build/entrance/firmware.bin
```

### Flash each ESP32

Connect the ESP32 to your laptop via USB, then run the matching command.
Flash one ESP32 at a time.

**Living Room ESP32:**
```bash
cd firmware
pio run -e living_room --target upload
```

**Bedroom ESP32:**
```bash
pio run -e bedroom --target upload
```

**Kitchen ESP32:**
```bash
pio run -e kitchen --target upload
```

**Entrance ESP32:**
```bash
pio run -e entrance --target upload
```

### Watch serial output (for debugging)

After flashing, keep the USB connected and run:
```bash
pio device monitor -e living_room    # replace with the room you want to watch
```

You should see WiFi connected and MQTT connected printed to the terminal.

### Useful extra commands

```bash
pio run --target clean               # delete build artifacts
pio run -e bedroom                   # compile one node without flashing
pio run -e kitchen --target upload && pio device monitor -e kitchen   # flash + monitor in one step
```

---

## 3. Raspberry Pi Hub (C++ Bridge + Automation Engine)

Both C++ programs are built from `hub/` using CMake.
Run these commands **on the Raspberry Pi** (SSH in first).

### Prerequisites — install once on the RPi

```bash
sudo apt update
sudo apt install -y cmake build-essential libssl-dev git
```

### Copy the project to the RPi

From your laptop:
```bash
scp -r /path/to/SmarterHome pi@smarthome-hub.local:~/SmarterHome
```

Or clone from git if you have it hosted remotely.

### Before building — fill in your AWS settings

Edit `hub/bridge/src/config.h`:
```cpp
static constexpr const char* AWS_ENDPOINT    = "XXXX-ats.iot.us-east-1.amazonaws.com";
static constexpr const char* AWS_CA_CERT     = "../certs/AmazonRootCA1.pem";
static constexpr const char* AWS_DEVICE_CERT = "../certs/certificate.pem.crt";
static constexpr const char* AWS_PRIVATE_KEY = "../certs/private.pem.key";
```

The endpoint is in `aws/certs/endpoint.txt` after running `aws/setup/provision.py`.
The cert files come from `aws/certs/smarthome-hub/` — copy them to `hub/certs/`:
```bash
cp aws/certs/smarthome-hub/* hub/certs/
cp aws/certs/AmazonRootCA1.pem hub/certs/
```

### Compile both binaries

```bash
cd ~/SmarterHome/hub
cmake -B build
cmake --build build -j$(nproc)
```

First build takes several minutes (downloads and compiles Paho MQTT C++ from source).
Subsequent builds are fast.

Binaries will be at:
```
hub/build/bridge/smarthome_bridge           ← MQTT bridge (local ↔ AWS)
hub/build/automation/smarthome_automation   ← local automation engine
```

### Run manually (for testing)

Open two SSH sessions and run one in each:
```bash
# Session 1
./hub/build/bridge/smarthome_bridge

# Session 2
./hub/build/automation/smarthome_automation
```

You should see `Connected to local Mosquitto broker` and `Connected to AWS IoT Core`.

### Install as systemd services (run automatically on boot)

```bash
sudo cp ~/SmarterHome/hub/config/smarthome-bridge.service /etc/systemd/system/
sudo cp ~/SmarterHome/hub/config/smarthome-automation.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable smarthome-bridge smarthome-automation
sudo systemctl start smarthome-bridge smarthome-automation
```

**Check status:**
```bash
sudo systemctl status smarthome-bridge
sudo systemctl status smarthome-automation
```

**View logs:**
```bash
journalctl -u smarthome-bridge -f
journalctl -u smarthome-automation -f
```

**Restart after recompiling:**
```bash
sudo systemctl restart smarthome-bridge smarthome-automation
```

---

## 4. Mobile App (Flutter)

### Prerequisites — install once on your laptop

1. Install Flutter SDK from `flutter.dev`
2. Install Android Studio (for Android) or Xcode (for iOS)
3. Run `flutter doctor` and follow any instructions it gives

### Before running — fill in your AWS settings

Edit `mobile_app/smarter_home/lib/services/auth_service.dart`:
```dart
const _userPoolId = 'us-east-1_XXXXXXXXX';   // from AWS Cognito
const _clientId   = 'XXXXXXXXXXXXXXXXXX';     // from AWS Cognito
```

Edit `mobile_app/smarter_home/lib/services/mqtt_service.dart`:
```dart
const _awsEndpoint = 'XXXX-ats.iot.us-east-1.amazonaws.com';
```

### Run on a connected phone or emulator

```bash
cd mobile_app/smarter_home
flutter pub get          # download dependencies (once)
flutter run              # build and launch on connected device
```

### Build a release APK (Android)

```bash
flutter build apk --release
# Output: mobile_app/smarter_home/build/app/outputs/flutter-apk/app-release.apk
```

Install on your phone:
```bash
flutter install          # installs the release APK on connected Android device
```

### Build for iOS

```bash
flutter build ios --release
# Open mobile_app/smarter_home/ios/Runner.xcworkspace in Xcode to archive and deploy
```

---

## Full Deployment Checklist

Follow this order — each step depends on the previous one.

- [ ] **1.** Flash Raspberry Pi OS onto microSD and boot the RPi
- [ ] **2.** SSH into RPi, run `aws/setup/provision.py` from your laptop to create AWS resources
- [ ] **3.** Copy hub certs to `hub/certs/` on the RPi
- [ ] **4.** Fill in `hub/bridge/src/config.h` with your AWS endpoint
- [ ] **5.** Build hub C++ binaries on the RPi (`cmake -B build && cmake --build build`)
- [ ] **6.** Install and start hub systemd services
- [ ] **7.** Fill in Wi-Fi + MQTT broker IP in `firmware/lib/smarthome/smarthome.h`
- [ ] **8.** Flash each ESP32 via USB (`pio run -e <room> --target upload`)
- [ ] **9.** Verify each ESP32 shows `online: true` in MQTT (`mosquitto_sub -h localhost -t 'smarthome/#' -v`)
- [ ] **10.** Verify bridge is forwarding to AWS (AWS Console → IoT Core → MQTT test client → subscribe to `smarthome/#`)
- [ ] **11.** Fill in Cognito and AWS endpoint in the Flutter app
- [ ] **12.** Run the mobile app and test end-to-end control

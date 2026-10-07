# SmarterHome — Full Project Explainer

This document explains every part of the project in plain language.
It will be updated as the project evolves.

---

## What Are We Building?

A smart home system that lets you control your home (lights, AC, door lock, etc.)
from your phone on your local Wi-Fi network.

The system has two layers:

```
┌─────────────────────────────────────────────────────────────┐
│  LAYER 2 — HOME HUB (Raspberry Pi)                          │
│  The brain of the home. Runs 24/7. Manages all the          │
│  small devices. Runs local automation rules.                │
│  MQTT Dash app on your phone connects here directly.        │
└──────────────────────────┬──────────────────────────────────┘
                           │ Wi-Fi (MQTT)
┌──────────────────────────▼──────────────────────────────────┐
│  LAYER 1 — SENSORS & ACTUATORS (ESP32 nodes)                │
│  Small microcontrollers in each room. They directly         │
│  control relays, read sensors, and fire IR signals.         │
└─────────────────────────────────────────────────────────────┘
```

> **Note:** AWS cloud integration (for remote access away from home)
> can be added later without changing anything in the current setup.

---

## What is an ESP32?

The ESP32 is a small, cheap microcontroller (~$5) made by Espressif.
Think of it like a tiny computer that:
- Has built-in Wi-Fi and Bluetooth
- Runs a single program in an infinite loop
- Can control physical hardware (turn relays on/off, read sensors)
- Uses very little power

We have 4 of them — one per room. Each runs a C++ program (firmware)
that we write and flash onto it.

**Why not use a Raspberry Pi for every room?**
RPi is overkill (and expensive) for just toggling a relay. ESP32 is
purpose-built for this kind of simple, always-on hardware control.

---

## What is a Raspberry Pi?

A Raspberry Pi is a full single-board computer (~$35–$75) that runs Linux.
Unlike the ESP32, it can run multiple programs, connect to the internet,
manage files, and act as a server.

In our project it plays the role of the **central hub** — the middleman
between the ESP32 nodes and AWS.

It runs:
1. **Mosquitto** — an MQTT broker (message router)
2. **bridge.py** — forwards messages between the home and AWS
3. **automation.py** — local rules engine

---

## What is MQTT?

MQTT is a lightweight messaging protocol designed for IoT devices.
It works on a **publish/subscribe** model:

- Devices **publish** messages to a **topic** (like a channel name)
- Other devices **subscribe** to topics and receive those messages
- A central **broker** (Mosquitto on the RPi) routes messages between them

Example:
```
Phone publishes  →  smarthome/living_room/light/1/set  →  {"state": "on"}
                         ↓ (broker routes it)
ESP32 receives   →  turns on relay → light turns on
```

Topics are just strings with `/` separators — like file paths.
Our convention: `smarthome/{room}/{device}/{action}`

---

## What is AWS IoT Core? (future)

AWS IoT Core is Amazon's managed MQTT broker in the cloud — accessible
from anywhere on the internet. This is not part of the current setup but
can be added later to enable remote access from outside your home network.

---

## What is the Automation Engine? (`hub/automation/`)

A local rules engine written in **C++**. It subscribes to sensor topics
on Mosquitto and automatically triggers actions based on rules you define.

Rules are defined in `hub/automation/src/rules.h` as a simple list —
add or remove rules there with no changes needed anywhere else.

Current rules:
- Motion at entrance → turn on living room light
- Kitchen gas alert → broadcast alert to all listeners
- Bedroom temp > 28°C → auto-start AC at 22°C
- Doorbell pressed → publish doorbell alert

**Why local?** If your internet goes down, AWS is unreachable and the
bridge stops working. But the automation engine talks only to local
Mosquitto — so these rules still fire even without internet.

Built alongside the bridge from the root `hub/CMakeLists.txt`.

---

## What is the Flutter App?

Flutter is Google's UI framework for building mobile apps from a single
codebase that runs on both iOS and Android.

Our app has:
- **Login screen** — authenticates via AWS Cognito
- **Home screen** — shows all rooms with online/offline status
- **Room screens** — controls specific to each room

The app connects to **AWS IoT Core** directly (not to the RPi) using
MQTT over WebSocket. This means it works from anywhere — home Wi-Fi
or mobile data.

---

## What is AWS Cognito?

Cognito is Amazon's user authentication service. It handles:
- User registration and login (email + password)
- Secure token generation (JWT tokens)
- Session management

Instead of building our own auth system, we use Cognito so we don't
have to store passwords ourselves.

---

## The Full Flow — Example: Turning On a Light from Abroad

1. You open the app on your phone (in another city)
2. App is connected to AWS IoT Core via MQTT WebSocket
3. You tap "Light 1 ON" in the Living Room screen
4. App publishes: `smarthome/living_room/light/1/set` → `{"state":"on"}`
5. AWS IoT Core receives it and routes to all subscribers
6. `bridge.py` on the RPi is subscribed → receives the message
7. Bridge republishes to local Mosquitto
8. Living Room ESP32 is subscribed locally → receives it
9. ESP32 sets GPIO 26 LOW → relay energizes → light turns on

Total latency: typically 100–300ms.

---

## Project File Structure

```
SmarterHome/
│
├── firmware/                   ← C++ programs that run ON the ESP32s
│   ├── living_room/            ← Lights + IR blaster + energy sensor
│   ├── bedroom/                ← Lights + IR blaster + temp/humidity
│   ├── kitchen/                ← Light + gas sensor + energy sensor
│   └── entrance/               ← Door lock + PIR motion + doorbell
│
├── hub/                        ← C++ programs that run ON the Raspberry Pi
│   ├── CMakeLists.txt          ← Root build file
│   ├── automation/             ← Local automation rules engine
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── main.cpp        ← Rule engine loop
│   │       └── rules.h         ← All automation rules defined here
│   └── config/
│       ├── mosquitto.conf      ← MQTT broker configuration
│       └── smarthome-automation.service  ← systemd service (auto-start on boot)
│
├── mqtt_dash/
│   └── smarthome_dashboard.json  ← Import into MQTT Dash app for instant dashboard
│
├── docs/
│   ├── project_explainer.md    ← This file
│   ├── setup_guide.md          ← Step-by-step deployment instructions
│   └── compile_and_flash.md    ← How to compile and flash every component
│
├── images/
│   └── download_rpi_os.sh      ← Downloads Raspberry Pi OS Lite image
│
└── .gitignore                  ← Excludes build artifacts and large files
```

---

## Hardware Summary

| Device | Count | Role | Key Components |
|--------|-------|------|----------------|
| Raspberry Pi 4 | 1 | Central hub | Runs Linux, Mosquitto, C++ automation |
| ESP32 — Living Room | 1 | Lights + climate + energy | 4-relay, IR LED, ACS712 |
| ESP32 — Bedroom | 1 | Lights + climate + climate sensor | 2-relay, IR LED, DHT22 |
| ESP32 — Kitchen | 1 | Light + safety + energy | relay, MQ-2 gas sensor, ACS712 |
| ESP32 — Entrance | 1 | Security | relay lock, PIR, doorbell button, buzzer |

---

## Key Sensors & Actuators Explained

**Relay** — An electrically controlled switch. The ESP32 sends a signal
(3.3V) to the relay module, which switches a 220V circuit on or off.
This is how we control regular home lights and appliances safely.

**IR Blaster (IR LED)** — Emits infrared light, exactly like your TV
remote. We use it to send "power on", "set temperature to 22°C" etc.
to AC units and TVs without any physical modification to the appliance.

**DHT22** — Measures temperature and humidity. Digital sensor, connects
with a single data wire.

**ACS712** — Current sensor. Measures how many amps are flowing through
a wire by detecting the magnetic field. From amps + voltage we calculate
watts (power consumption).

**MQ-2** — Gas sensor. Detects LPG, propane, methane, smoke. Outputs
an analog voltage proportional to gas concentration.

**PIR (HC-SR501)** — Passive Infrared motion sensor. Detects body heat
movement. Outputs HIGH when motion is detected.

---

## What's Next

- [ ] Flash Raspberry Pi OS and set up Mosquitto
- [ ] Build and deploy automation engine on RPi
- [ ] Flash all 4 ESP32 nodes
- [ ] Set up MQTT Dash and test end-to-end
- [ ] (Later) Add AWS IoT Core for remote access

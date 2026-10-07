# SmarterHome — Full Project Explainer

This document explains every part of the project in plain language.
It will be updated as the project evolves.

---

## What Are We Building?

A smart home system that lets you control your home (lights, AC, door lock, etc.)
from your phone — even when you're away from home.

The system has three layers:

```
┌─────────────────────────────────────────────────────────────┐
│  LAYER 3 — CLOUD (AWS IoT Core)                             │
│  Acts as the relay between your phone and your home.        │
│  When you're away, your phone talks to AWS, AWS talks       │
│  to your home.                                              │
└──────────────────────────┬──────────────────────────────────┘
                           │ Internet (MQTT over WebSocket)
┌──────────────────────────▼──────────────────────────────────┐
│  LAYER 2 — HOME HUB (Raspberry Pi)                          │
│  The brain of the home. Runs 24/7. Manages all the          │
│  small devices. Also runs local automation rules so          │
│  things still work if the internet goes down.               │
└──────────────────────────┬──────────────────────────────────┘
                           │ Wi-Fi (MQTT)
┌──────────────────────────▼──────────────────────────────────┐
│  LAYER 1 — SENSORS & ACTUATORS (ESP32 nodes)                │
│  Small microcontrollers in each room. They directly         │
│  control relays, read sensors, and fire IR signals.         │
└─────────────────────────────────────────────────────────────┘
```

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

## What is AWS IoT Core?

AWS IoT Core is Amazon's managed MQTT broker in the cloud.
It's the same concept as Mosquitto, but running on AWS servers,
accessible from anywhere on the internet.

It also supports **Thing Shadows** — a JSON document that stores the
last known state of each device. So if your phone queries "is the
bedroom light on?", AWS can answer immediately without needing to
ping the ESP32.

**Authentication:** AWS IoT uses X.509 certificates (like HTTPS) to
verify that only your devices can connect. Each device gets its own
certificate.

---

## What is the Bridge Daemon? (`hub/bridge/`)

The bridge is a **C++ program** that runs on the RPi and connects to
both brokers simultaneously using the Paho MQTT C++ library:

```
Local Mosquitto ←──── smarthome_bridge (C++) ────→ AWS IoT Core
```

- When an ESP32 publishes a sensor reading locally, the bridge
  forwards it to AWS → your phone sees it.
- When you tap a button in the app, it publishes to AWS → the bridge
  receives it and republishes locally → the ESP32 acts on it.

It also prevents **echo loops** — without that protection, a message
forwarded to AWS would come back from AWS and be forwarded to local again,
forever.

It is built with CMake on the Raspberry Pi and runs as a systemd service.
CMake automatically downloads and builds Paho MQTT C++ via FetchContent —
no manual library installation needed.

---

## What is the Automation Engine? (`hub/automation.py`)

A local rules engine. It subscribes to sensor topics on Mosquitto and
automatically triggers actions based on rules you define.

Current rules:
- Motion at entrance → turn on living room light
- Kitchen gas alert → broadcast alert to all listeners
- Bedroom temp > 28°C → auto-start AC at 22°C
- Doorbell pressed → publish doorbell alert

**Why local?** If your internet goes down, AWS is unreachable and the
bridge stops working. But the automation engine talks only to local
Mosquitto — so these rules still fire even without internet.

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
├── hub/                        ← Python programs that run ON the Raspberry Pi
│   ├── bridge.py               ← Mosquitto ↔ AWS IoT Core bridge
│   ├── automation.py           ← Local automation rules engine
│   └── config/
│       ├── mosquitto.conf      ← MQTT broker configuration
│       ├── requirements.txt    ← Python dependencies
│       └── *.service           ← systemd service files (auto-start on boot)
│
├── aws/
│   ├── setup/
│   │   ├── provision.py        ← Creates AWS IoT Things + certificates
│   │   └── teardown.py         ← Deletes all AWS resources (use with care)
│   └── certs/                  ← Generated by provision.py (gitignored)
│
├── mobile_app/
│   └── smarter_home/           ← Flutter app (iOS + Android)
│       └── lib/
│           ├── main.dart               ← App entry point
│           ├── services/
│           │   ├── auth_service.dart   ← AWS Cognito login/logout
│           │   └── mqtt_service.dart   ← MQTT connection + publish/subscribe
│           └── screens/
│               ├── login_screen.dart   ← Login UI
│               ├── home_screen.dart    ← Room grid + alert banner
│               └── room_screen.dart    ← Per-room controls
│
├── docs/
│   ├── project_explainer.md    ← This file
│   └── setup_guide.md          ← Step-by-step deployment instructions
│
├── scripts/
│   └── download_rpi_os.sh      ← Downloads Raspberry Pi OS Lite image
│
└── .gitignore                  ← Excludes certificates and build artifacts
```

---

## Hardware Summary

| Device | Count | Role | Key Components |
|--------|-------|------|----------------|
| Raspberry Pi 4 | 1 | Central hub | Runs Linux, Mosquitto, Python |
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

- [ ] Convert firmware from `.ino` to `.cpp` with PlatformIO project structure
- [ ] Set up Raspberry Pi OS (see `scripts/download_rpi_os.sh`)
- [ ] Deploy and test end-to-end

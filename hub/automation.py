#!/usr/bin/env python3
"""
SmarterHome — Local Automation Engine

Runs on the Raspberry Pi and subscribes to sensor events on the local
Mosquitto broker. Executes automation rules without needing AWS to be
reachable, so the home still works during internet outages.

Rules are defined in the RULES list below. Each rule has:
  - trigger_topic: MQTT topic to watch
  - trigger_fn:    function(payload_dict) → bool, returns True to fire
  - action_topic:  topic to publish to
  - action_payload: dict to publish (or callable(payload) → dict)

Run:
    python automation.py

Run as systemd service alongside bridge.py.
"""

import json
import logging
import time

import paho.mqtt.client as mqtt

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [AUTOMATION] %(message)s",
)
log = logging.getLogger(__name__)

LOCAL_BROKER = "localhost"
LOCAL_PORT = 1883
CLIENT_ID = "smarthome-automation"

# ── Automation Rules ───────────────────────────────────────────────────────────
# Each entry: (trigger_topic, trigger_fn, action_topic, action_payload)
# trigger_fn receives the decoded JSON payload dict.
# action_payload can be a dict or a callable(payload) → dict.

RULES = [
    # Motion at entrance → turn on entrance light (relay 1 on living room... adjust as needed)
    {
        "trigger_topic": "smarthome/entrance/motion/event",
        "trigger_fn": lambda p: p.get("detected") is True,
        "action_topic": "smarthome/living_room/light/1/set",
        "action_payload": {"state": "on"},
        "description": "Motion detected at entrance → turn on living room light",
    },
    # Gas alert in kitchen → send alert (ESP32 already triggers buzzer locally)
    {
        "trigger_topic": "smarthome/kitchen/gas/status",
        "trigger_fn": lambda p: p.get("alert") is True,
        "action_topic": "smarthome/alerts/gas",
        "action_payload": lambda p: {"room": "kitchen", "ppm": p.get("ppm"), "alert": True},
        "description": "Gas alert in kitchen → broadcast alert topic",
    },
    # High temperature in bedroom → turn on AC at 22°C
    {
        "trigger_topic": "smarthome/bedroom/climate/status",
        "trigger_fn": lambda p: p.get("temp", 0) > 28,
        "action_topic": "smarthome/bedroom/ac/set",
        "action_payload": {"mode": "cool", "temp": 22},
        "description": "Bedroom temp > 28°C → auto-cool to 22°C",
    },
    # Doorbell pressed → notify hub status topic (app shows push notification)
    {
        "trigger_topic": "smarthome/entrance/doorbell/event",
        "trigger_fn": lambda p: p.get("pressed") is True,
        "action_topic": "smarthome/alerts/doorbell",
        "action_payload": {"message": "Someone is at the door!"},
        "description": "Doorbell pressed → publish doorbell alert",
    },
]

# ── Rule Engine ────────────────────────────────────────────────────────────────
# Track last fire time per rule to avoid rapid re-triggering
_last_fired: dict = {}
RULE_COOLDOWN_SECONDS = 10


class AutomationEngine:
    def __init__(self):
        self.client = mqtt.Client(client_id=CLIENT_ID)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

        # Build a map: topic → list of rules
        self.topic_rules: dict = {}
        for i, rule in enumerate(RULES):
            topic = rule["trigger_topic"]
            self.topic_rules.setdefault(topic, []).append((i, rule))

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            log.info("Connected to local Mosquitto broker")
            for topic in self.topic_rules:
                client.subscribe(topic)
                log.info("Subscribed to: %s", topic)
        else:
            log.error("Connection failed: rc=%d", rc)

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
        except (json.JSONDecodeError, UnicodeDecodeError):
            return

        topic = msg.topic
        for rule_idx, rule in self.topic_rules.get(topic, []):
            try:
                should_fire = rule["trigger_fn"](payload)
            except Exception as e:
                log.warning("Rule %d trigger_fn error: %s", rule_idx, e)
                continue

            if not should_fire:
                continue

            # Cooldown check
            now = time.time()
            if now - _last_fired.get(rule_idx, 0) < RULE_COOLDOWN_SECONDS:
                continue
            _last_fired[rule_idx] = now

            # Resolve action payload
            action_payload = rule["action_payload"]
            if callable(action_payload):
                try:
                    action_payload = action_payload(payload)
                except Exception as e:
                    log.warning("Rule %d action_payload error: %s", rule_idx, e)
                    continue

            action_topic = rule["action_topic"]
            self.client.publish(action_topic, json.dumps(action_payload), qos=1)
            log.info("Rule fired: %s", rule.get("description", f"Rule {rule_idx}"))

    def run(self):
        self.client.connect(LOCAL_BROKER, LOCAL_PORT)
        log.info("Automation engine running. Ctrl+C to stop.")
        try:
            self.client.loop_forever()
        except KeyboardInterrupt:
            pass
        finally:
            self.client.disconnect()
            log.info("Automation engine stopped.")


if __name__ == "__main__":
    AutomationEngine().run()

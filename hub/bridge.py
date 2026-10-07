#!/usr/bin/env python3
"""
SmarterHome — Raspberry Pi Bridge Daemon

Bridges the local Mosquitto MQTT broker ↔ AWS IoT Core.

- Subscribes to smarthome/# on local Mosquitto
- Forwards messages to AWS IoT Core (same topic)
- Subscribes to smarthome/# on AWS IoT Core
- Forwards messages down to local Mosquitto (for remote app control)

This means the mobile app talks to AWS IoT Core, and the bridge
relays commands down to the ESP32 nodes via local MQTT.

Setup:
    pip install -r config/requirements.txt
    # Copy certs from aws/certs/smarthome-hub/ into hub/certs/
    # Update CONFIG below to match your setup

Run:
    python bridge.py

Run as systemd service:
    sudo cp config/smarthome-bridge.service /etc/systemd/system/
    sudo systemctl enable smarthome-bridge
    sudo systemctl start smarthome-bridge
"""

import json
import logging
import os
import time
import threading

import paho.mqtt.client as mqtt
from awscrt import io, mqtt as aws_mqtt
from awsiot import mqtt_connection_builder

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger(__name__)

# ── Configuration ──────────────────────────────────────────────────────────────
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
CERTS_DIR = os.path.join(BASE_DIR, "certs")

CONFIG = {
    # Local Mosquitto
    "local_host": "localhost",
    "local_port": 1883,
    # AWS IoT Core endpoint (from aws/certs/endpoint.txt after provisioning)
    "aws_endpoint": open(os.path.join(BASE_DIR, "../aws/certs/endpoint.txt")).read().strip(),
    # Certificates for the hub thing
    "cert": os.path.join(CERTS_DIR, "certificate.pem.crt"),
    "key": os.path.join(CERTS_DIR, "private.pem.key"),
    "ca": os.path.join(CERTS_DIR, "AmazonRootCA1.pem"),
    "client_id": "smarthome-hub",
    # Topics to bridge (bidirectional)
    "topic_filter": "smarthome/#",
    "aws_qos": aws_mqtt.QoS.AT_LEAST_ONCE,
}

# ── Shared state ───────────────────────────────────────────────────────────────
_local_client = None
_aws_connection = None
# Prevent echo loops: track in-flight message IDs
_forwarding_lock = threading.Lock()
_in_flight_local_to_aws: set = set()
_in_flight_aws_to_local: set = set()


# ── Local MQTT → AWS ───────────────────────────────────────────────────────────
def on_local_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload

    # Skip messages that originated from AWS (avoid echo loop)
    key = (topic, payload)
    with _forwarding_lock:
        if key in _in_flight_aws_to_local:
            _in_flight_aws_to_local.discard(key)
            return
        _in_flight_local_to_aws.add(key)

    log.debug("Local → AWS: %s", topic)
    _aws_connection.publish(
        topic=topic,
        payload=payload,
        qos=CONFIG["aws_qos"],
    )


def on_local_connect(client, userdata, flags, rc):
    if rc == 0:
        log.info("Connected to local Mosquitto broker")
        client.subscribe(CONFIG["topic_filter"])
    else:
        log.error("Failed to connect to local Mosquitto: rc=%d", rc)


def on_local_disconnect(client, userdata, rc):
    log.warning("Disconnected from local Mosquitto (rc=%d), reconnecting...", rc)


# ── AWS IoT Core → Local ───────────────────────────────────────────────────────
def on_aws_message(topic, payload, **kwargs):
    key = (topic, payload)
    with _forwarding_lock:
        if key in _in_flight_local_to_aws:
            _in_flight_local_to_aws.discard(key)
            return
        _in_flight_aws_to_local.add(key)

    log.debug("AWS → Local: %s", topic)
    _local_client.publish(topic, payload, qos=1)


# ── Main ───────────────────────────────────────────────────────────────────────
def build_local_client():
    client = mqtt.Client(client_id="smarthome-bridge-local")
    client.on_connect = on_local_connect
    client.on_message = on_local_message
    client.on_disconnect = on_local_disconnect
    return client


def build_aws_connection():
    event_loop_group = io.EventLoopGroup(1)
    host_resolver = io.DefaultHostResolver(event_loop_group)
    client_bootstrap = io.ClientBootstrap(event_loop_group, host_resolver)

    connection = mqtt_connection_builder.mtls_from_path(
        endpoint=CONFIG["aws_endpoint"],
        cert_filepath=CONFIG["cert"],
        pri_key_filepath=CONFIG["key"],
        client_bootstrap=client_bootstrap,
        ca_filepath=CONFIG["ca"],
        client_id=CONFIG["client_id"],
        clean_session=False,
        keep_alive_secs=30,
    )
    return connection


def main():
    global _local_client, _aws_connection

    # Build and connect local MQTT client
    _local_client = build_local_client()
    _local_client.connect(CONFIG["local_host"], CONFIG["local_port"])
    _local_client.loop_start()

    # Build and connect AWS IoT connection
    _aws_connection = build_aws_connection()
    log.info("Connecting to AWS IoT Core at %s...", CONFIG["aws_endpoint"])
    connect_future = _aws_connection.connect()
    connect_future.result()
    log.info("Connected to AWS IoT Core")

    # Subscribe to all smarthome topics on AWS
    subscribe_future, _ = _aws_connection.subscribe(
        topic=CONFIG["topic_filter"],
        qos=CONFIG["aws_qos"],
        callback=on_aws_message,
    )
    subscribe_future.result()
    log.info("Subscribed to '%s' on AWS IoT Core", CONFIG["topic_filter"])

    # Publish hub online status
    _aws_connection.publish(
        topic="smarthome/hub/status",
        payload=json.dumps({"online": True}),
        qos=CONFIG["aws_qos"],
    )

    log.info("Bridge running. Ctrl+C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        log.info("Shutting down bridge...")
        _local_client.loop_stop()
        _local_client.disconnect()
        disconnect_future = _aws_connection.disconnect()
        disconnect_future.result()
        log.info("Bridge stopped.")


if __name__ == "__main__":
    main()

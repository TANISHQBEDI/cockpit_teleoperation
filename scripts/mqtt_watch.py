#!/usr/bin/env python3
"""Subscribe to the CS command.json stream on the site MQTT / VDA 5050 bus.

This is a client. It does not start a broker. Any MQTT 3.1.1 broker works
(HiveMQ, EMQX, Mosquitto, AWS IoT Core, the ANSCER FMS bus, …).

Python MQTT client options (pick one; this script uses Paho):

  Package       Pros                         Cons
  paho-mqtt     Eclipse, any broker, simple  Sync loop; v1 vs v2 API
  gmqtt         Async, MQTT 5                More code for a watch tool
  aiomqtt       Async context managers       Extra asyncio boilerplate

  pip install paho-mqtt
  # or: sudo apt-get install python3-paho-mqtt   (Ubuntu)

Examples
  python3 scripts/mqtt_watch.py --host 192.168.1.10 --port 1883
  MQTT_BROKER=tcp://192.168.1.10:1883 python3 scripts/mqtt_watch.py
  python3 scripts/mqtt_watch.py --host BROKER --topic 'uagv/v2/+/+/command'
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import sys
import time
from typing import Any, Optional
from urllib.parse import urlparse

try:
    import paho.mqtt.client as mqtt
except ImportError:
    sys.stderr.write(
        "paho-mqtt is not installed.\n"
        "  python3 -m venv .venv && . .venv/bin/activate && pip install paho-mqtt\n"
        "  # Ubuntu apt: sudo apt-get install python3-paho-mqtt\n"
    )
    sys.exit(2)


DEFAULT_TOPIC = "uagv/v2/ANSCER/AR001/command"


def parse_broker_uri(uri: str) -> tuple[str, int]:
    raw = uri.strip()
    if "://" not in raw:
        raw = "tcp://" + raw
    parsed = urlparse(raw)
    host = parsed.hostname or "127.0.0.1"
    port = parsed.port or 1883
    return host, port


def make_client(client_id: str) -> mqtt.Client:
    if hasattr(mqtt, "CallbackAPIVersion"):
        return mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            protocol=mqtt.MQTTv311,
        )
    return mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)


class Watcher:
    def __init__(self, pretty: bool, max_messages: int) -> None:
        self.pretty = pretty
        self.max_messages = max_messages
        self.count = 0
        self.window_count = 0
        self.window_start = time.monotonic()
        self.running = True

    def on_connect(self, client: mqtt.Client, _ud: Any, _flags: Any, reason_code: Any,
                   _properties: Any = None) -> None:
        rc = reason_code
        if hasattr(reason_code, "value"):
            rc = reason_code.value
        if rc == 0:
            sys.stderr.write("[mqtt_watch] connected\n")
        else:
            sys.stderr.write(f"[mqtt_watch] connect failed rc={rc}\n")

    def on_message(self, _client: mqtt.Client, _ud: Any, msg: mqtt.MQTTMessage) -> None:
        self.count += 1
        self.window_count += 1
        payload = msg.payload.decode("utf-8", errors="replace")
        if self.pretty:
            try:
                payload = json.dumps(json.loads(payload), indent=2)
            except json.JSONDecodeError:
                pass
        sys.stdout.write(f"\n--- {msg.topic} qos={msg.qos} #{self.count} ---\n")
        sys.stdout.write(payload)
        if not payload.endswith("\n"):
            sys.stdout.write("\n")
        sys.stdout.flush()
        if 0 < self.max_messages <= self.count:
            self.running = False

    def maybe_rate(self) -> None:
        now = time.monotonic()
        elapsed = now - self.window_start
        if elapsed >= 1.0:
            sys.stderr.write(
                f"[mqtt_watch] rate={self.window_count / elapsed:.1f} Hz  "
                f"total={self.count}\n"
            )
            self.window_count = 0
            self.window_start = now


def main() -> int:
    env_broker = os.environ.get("MQTT_BROKER", "")
    env_host, env_port = (
        parse_broker_uri(env_broker) if env_broker else ("127.0.0.1", 1883)
    )

    parser = argparse.ArgumentParser(
        description="Watch command.json on the site MQTT / VDA 5050 bus."
    )
    parser.add_argument("--host", default=os.environ.get("MQTT_HOST", env_host))
    parser.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", env_port)))
    parser.add_argument(
        "--topic",
        default=os.environ.get("MQTT_TOPIC", DEFAULT_TOPIC),
        help="MQTT topic or filter (+ and # allowed)",
    )
    parser.add_argument("--username", default=os.environ.get("MQTT_USERNAME", ""))
    parser.add_argument("--password", default=os.environ.get("MQTT_PASSWORD", ""))
    parser.add_argument(
        "--pretty",
        action="store_true",
        help="Indent JSON (do not use while measuring 500 Hz)",
    )
    parser.add_argument(
        "--max-messages",
        type=int,
        default=0,
        help="Exit after N messages (0 = run until Ctrl+C)",
    )
    parser.add_argument(
        "--client-id",
        default=os.environ.get("MQTT_CLIENT_ID", "cs-g29-watch"),
    )
    args = parser.parse_args()

    watcher = Watcher(pretty=args.pretty, max_messages=args.max_messages)
    client = make_client(args.client_id)
    client.on_connect = watcher.on_connect
    client.on_message = watcher.on_message
    if args.username:
        client.username_pw_set(args.username, args.password or None)

    def stop(_sig: int, _frame: Optional[object]) -> None:
        watcher.running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    sys.stderr.write(
        f"[mqtt_watch] {args.host}:{args.port} topic={args.topic} "
        f"(MQTT 3.1.1, any broker)\n"
    )
    try:
        client.connect(args.host, args.port, keepalive=30)
    except OSError as exc:
        sys.stderr.write(f"[mqtt_watch] cannot reach {args.host}:{args.port}: {exc}\n")
        return 1

    client.subscribe(args.topic, qos=0)
    client.loop_start()
    try:
        while watcher.running:
            watcher.maybe_rate()
            time.sleep(0.2)
    finally:
        client.loop_stop()
        client.disconnect()
        sys.stderr.write(f"[mqtt_watch] stop total={watcher.count}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

# CS G29 reader (Step 1)

Control Station process that reads a Logitech G29 and publishes ANSCER `command.json` at 500 Hz to the **existing** site MQTT broker. We do not run a broker in this repo.

Pipeline this repo will grow into:

```
G29 evdev  →  command.json  →  MQTT  →  ROS (Noetic) on the AMR
AMR        →  feedback.json →  MQTT  →  CS UI / haptics
```

This directory is only the first box: **raw G29 → `command.json`**.

## What runs where

| Machine | What you can do | G29 USB |
| --- | --- | --- |
| This Mac + Docker | Build the Linux binary, run `--mock` at 500 Hz | No — Docker Desktop cannot pass HID into the VM |
| Ubuntu CS (ROS 1 Noetic host) | Same image, `--device auto` against the real wheel | Yes — bind `/dev/input` |

Do not try to read the G29 from macOS IOKit. The Control Station that will sit next to the wheel is Linux (`hid-lg4ff`, `/dev/input`, later force-feedback). The Mac is a compile/mock workstation.

## Limitations and alternatives

### USB / Docker

| Option | Mac | Ubuntu | Notes |
| --- | --- | --- | --- |
| **Docker `--device /dev/input` (chosen on Ubuntu)** | Does not work | Works | Docker Desktop has no USB HID passthrough |
| USB/IP into Docker Desktop | Fragile | N/A | Extra server, flaky HID |
| Native macOS HID reader | Would work on Mac only | N/A | Second stack; skipped |
| **`--mock` in the Linux image (chosen on Mac)** | Works | Works | Sweeps steering so JSON is not all zeros |

### Input API (inspiration: ROS `joy_linux` / `joy_node`)

| API | Rate | Steering resolution | Force-feedback later | Verdict |
| --- | --- | --- | --- | --- |
| **Linux evdev + libevdev (chosen)** | Event-driven + 500 Hz snapshot timer | Full ABS range (G29 wheel is 16-bit) | Same `event*` node | Matches how we will do FF |
| `/dev/input/js*` (ROS1 `joy_linux`) | Good | Often scaled/coalesced | Separate `event*` | Fine, but worse for G29 |
| SDL2 (ROS2 `joy`) | Good | Mapping-dependent | Indirect | Extra dep, not needed |
| hidapi userspace | Good | Raw HID | Possible | Bypasses `hid-lg4ff`; harder on Ubuntu |

Output is a **fixed 500 Hz snapshot** (like `joy_node` `autorepeat_rate`), not “publish only on change”. Coalesce-on-change would undershoot when the wheel is held still.

### Ubuntu 20.04 + C++20 + ROS 1 Noetic

| Base | ROS | C++20 | Support |
| --- | --- | --- | --- |
| **Ubuntu 20.04 + g++-10 (chosen)** | Add `ros:noetic` in a later image | Yes | 20.04 is ESM; matches Noetic |
| Ubuntu 22.04 + Humble | ROS 2 | Default g++ | Current LTS pairing; not selected |
| Default focal g++-9 | Noetic | Partial | Do not use |

The reader is a **standalone binary**. It does not link ROS. Step 3 (ROS test) can use `ros:noetic` and `mosquitto_sub` / a small subscriber without putting `roscpp` in this process.

### MQTT client (Step 2 — not linked yet)

| Library | Pros | Cons |
| --- | --- | --- |
| **Eclipse Paho MQTT C++** | Mature, async, TLS, MQTT 3.1.1 + 5, used in industry | Wraps Paho C |
| libmosquitto | Same project as the usual broker, tiny C API | C++ wrapper is deprecated |
| Boost.MQTT5 | Modern C++20 / Asio | Boost; MQTT 5 only |
| redboltz `mqtt_cpp` | Header-only, 3.1.1 + 5 | Less common in AMRs |

**Chosen client: libmosquitto** (Ubuntu 20.04 `libmosquitto-dev`). QoS 0. Point `mqtt_broker` / `--broker` at the FMS bus. `command.json` is an ANSCER teleop topic, not stock VDA 5050 `order` / `state`. Default topic: `uagv/v2/ANSCER/AR001/command`.

## Build and run (Mac — mock)

```bash
docker compose build
docker compose run --rm g29-reader --mock --duration 3 --json-stdout --rate 500
```

stderr prints 1 Hz stats (`rate=500 Hz`, `mqtt_stub=…`, `mqtt_connected=false`). stdout is one JSON object per tick. Pipe it if you do not want 1500 lines in the terminal:

```bash
docker compose run --rm g29-reader --mock --duration 2 --json-stdout \
  > /tmp/command.ndjson
```

The image built on Apple Silicon is `linux/arm64`. The Ubuntu Control Station is almost certainly `amd64`. After `git pull`, run `docker compose build` on Ubuntu. Do not copy the Mac image across.

`--json-stdout` piped through Docker on a Mac can show a few `overruns` (stdout stalls). On Ubuntu, omit `--json-stdout` for the 500 Hz path and inspect stderr stats; use `--json-stdout` only when capturing a short sample.

## Build and run (Ubuntu — real G29)

Physical switch on the wheel: **PS3 / PC** so Linux sees `046d:c24f`. PS4 mode is `046d:c260` and this reader refuses it.

```bash
# optional stable symlink
sudo cp udev/99-logitech-g29.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger

git pull
docker compose build
docker compose run --rm --device /dev/input:/dev/input g29-reader \
  --device auto \
  --config /etc/cs_g29_reader/g29_mapping.conf \
  --print-raw --rate 10
```

Once `--print-raw` shows the real button/axis codes, edit `config/g29_mapping.conf` (especially `steering_range_deg` and `btn_*`) and rebuild. Then drop `--print-raw` and use `--rate 500 --json-stdout`.

Host binary (no Docker), if you prefer:

```bash
sudo apt-get install -y g++-10 cmake make pkg-config libevdev-dev
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/g29_reader --device auto --config config/g29_mapping.conf
```

## Safety interlock in this step

| # | Check | Implemented now |
| --- | --- | --- |
| 1 | G29 present in PC mode (`046d:c24f`) | Yes — no JSON / no stub publish while `DISCONNECTED` |
| 2 | Robot MANUAL mode | Later (needs FMS / `feedback.json`) |
| 3 | WebRTC video | Later |
| 4 | E-stop CLEAR | Later |

## Config knobs to confirm on Ubuntu

`steering_range_deg` defaults to **900** (G29 native lock, ±450°, 0 = center). The spec example `89.6` looks like a smaller lock (e.g. 180°). Change the constant after the first `evtest`; do not guess.

Pedals default to **inverted** (kernel 0 = pressed → JSON 1.0). H-shifter is always `shifterGear: 0` until we add the optional USB shifter.

## Next steps (do not start until this mock run looks right)

2. MQTT client → existing site broker (QoS 0).
3. ROS 1 Noetic subscriber check on Ubuntu.
4. `feedback.json` + remaining interlocks.
5. Path projection from `velocity`.

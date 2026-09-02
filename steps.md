# Test steps

Do **only the current stage**. Later stages are added here when that code exists.

**Current stage: 1 — G29 → `command.json` (MQTT stub, not connected)**

Not in this stage: real MQTT, ROS, MANUAL / video / E-stop interlocks, haptics, path projection.

---

## Stage 1 — Mac (mock, no wheel)

Docker Desktop running. From the repo root:

```bash
docker compose build
docker compose run --rm g29-reader --mock --duration 3 --json-stdout --rate 500
```

**Pass**

- stderr: `source=mock`, `mqtt_connected=false`, `rate` near `500`
- stdout: JSON with `header` / `voltageVariedInputs` / `digitalButtons` / `optionalHShifter`
- `steeringAngleDeg` changes over the 3 seconds

**Fail:** Docker daemon not running, or you used a USB device flag (Mac cannot pass the G29 in).

This Mac image is `arm64`. Do not copy it to Ubuntu; rebuild there.

---

## Stage 1 — Ubuntu (real G29)

Copy this repo onto the machine (`git pull`, `scp`, or USB). Pedals in the **wheel**, not a second USB port. Switch on the wheel: **PS3 / PC**.

### 1. USB ID

```bash
lsusb | grep -i logitech
```

**Pass:** `046d:c24f`  
**Fail:** `046d:c260` → flip switch, unplug, replug.

### 2. Build

```bash
cd cockpit_teleoperation
docker compose build
```

No Docker:

```bash
sudo apt-get install -y g++-10 cmake make pkg-config libevdev-dev
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3. Kernel sees the wheel

```bash
docker compose run --rm --device /dev/input:/dev/input g29-reader --list-devices
```

Host: `./build/g29_reader --list-devices`

**Pass:** a line with `vendor=046d product=c24f` and `/dev/input/eventN`  
**Fail (empty in Docker):** drop Docker; use `./build/g29_reader` for the rest.

### 3b. Permission denied / empty `--list-devices` over SSH

`lsusb` seeing `046d:c24f` only means USB enumerated. Reading the wheel needs `/dev/input/event*`, which is `root:input` mode `0660`. Your login user is often **not** in `input`. On some Ubuntu images `/dev/input` itself is `750`, so you cannot even `ls` it.

On the **SSH host** (not inside Docker):

```bash
# kernel still lists it without the input group
grep -A8 -i -e g29 -e c24f /proc/bus/input/devices

ls -ld /dev/input
groups
getent group input
```

**Today (no logout):** use root for the reader.

```bash
# host binary
sudo ./build/g29_reader --list-devices
sudo ./build/g29_reader --device auto --dump-caps
sudo ./build/g29_reader --device auto --print-raw --rate 10 --json-stdout

# Docker — container is root, but it still needs the host nodes bound in
sudo docker compose run --rm --device /dev/input:/dev/input \
  g29-reader --list-devices
```

**Forever (this user):**

```bash
sudo usermod -aG input "$USER"
sudo cp udev/99-logitech-g29.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then **exit SSH and log in again**. `groups` must show `input`. `newgrp input` is not enough for every later terminal.

Optional: `sudo -- docker compose ... --group-add $(getent group input | cut -d: -f3)` if you run the container as a non-root user.

**Pass:** `--list-devices` prints the G29 with `open=ok` (after you rebuild) or `sudo` can `--dump-caps`.  
**Fail:** `lsusb` has the wheel but `grep G29 /proc/bus/input/devices` is empty → not a permission issue; try another USB port / unplug-replug.

### 4. Calibrate (10 Hz)

```bash
docker compose run --rm --device /dev/input:/dev/input g29-reader \
  --device auto \
  --config /etc/cs_g29_reader/g29_mapping.conf \
  --print-raw --rate 10 --json-stdout
```

Host: `./build/g29_reader --device auto --config config/g29_mapping.conf --print-raw --rate 10 --json-stdout`

Move **one** control at a time. Write `code=` from `[raw]` and check the JSON field.

| You do | JSON field |
| --- | --- |
| Steer L / center / R | `steeringAngleDeg` |
| Accel / brake / clutch | `acceleratorPedal` / `brakePedal` / `clutchPedal` (released ~0, floor ~1) |
| Face buttons, paddles, D-pad, +/− | matching bools; rotary changes `rotaryDialPosition` |

Verified on this wheel (2026-09-02, `046d:c24f`):

| Control | raw | conf key |
| --- | --- | --- |
| Steer | `ABS_X` code 0 | `steer_abs_code=0` |
| Accelerator | `ABS_Z` code 2 | `accel_abs_code=2` |
| Brake | `ABS_RZ` code 5 | `brake_abs_code=5` |
| Clutch | `ABS_Y` code 1 | `clutch_abs_code=1` |

Already in `config/g29_mapping.conf`. Re-run §4 after `git pull` (or edit the file on the Ubuntu box). One pedal at a time: only that JSON field should rise. Buttons still unverified.

If a field never moves, go to **§5** (do not assume the binary is broken). Edit `config/g29_mapping.conf` and re-run — compose mounts that file, no image rebuild.

- Full lock ~±450 → leave `steering_range_deg=900`
- Full lock ~±90 → set `180`
- Pedals inverted → `invert_pedals=0`

Do not use `DataStreaming/LogitechG29.py` (TCP CSV, ~33 Hz). `Logitech_g29_read.py` is optional extra printout only.

### 5. If the mapping is wrong (still test the system)

Wrong `btn_*` / axis codes do **not** mean the reader is broken. Split the test:

| Layer | Mapping needed? | How to test |
| --- | --- | --- |
| USB + open device | No | `--list-devices`, `046d:c24f` |
| 500 Hz loop + JSON shape + MQTT stub | No (use `--mock` anywhere, or real wheel even with bad names) | `--duration 5 --rate 500` — look at `emitted`, `blocked=0` |
| Field meaning (Cross vs Square, which pedal) | Yes | `--dump-caps` then `--print-raw` then edit conf |

**A. Pipeline still works (do this even if buttons look swapped)**

Mac or Ubuntu:

```bash
docker compose run --rm g29-reader --mock --duration 3 --json-stdout --rate 500
```

Ubuntu with wheel, ignore field names:

```bash
docker compose run --rm --device /dev/input:/dev/input g29-reader \
  --device auto --rate 500 --duration 5
```

**Pass:** `blocked=0`, `rate` near 500, `mqtt_connected=false`. JSON keys exist even if the wrong control moves `crossX`.

**B. What the kernel actually reports (Ubuntu only)**

```bash
docker compose run --rm --device /dev/input:/dev/input g29-reader \
  --device auto --dump-caps
```

Host: `./build/g29_reader --device auto --config config/g29_mapping.conf --dump-caps`

**Pass:** a list of `EV_ABS` codes with `min..max` and `EV_KEY` codes. Our guesses are printed first — if `btn_cross=288` is **not** in the EV_KEY list, that default is wrong for this kernel.

**C. One control → one code → one JSON field**

```bash
docker compose run --rm --device /dev/input:/dev/input g29-reader \
  --device auto --print-raw --rate 10 --json-stdout
```

1. Hands off. Note the idle JSON.
2. Move **only** the steering wheel. `[raw]` should show one `EV_ABS` `code=N`. That N is `steer_abs_code`. `steeringAngleDeg` in JSON should change. If JSON does not change but raw does, N is not `steer_abs_code` — set it to N.
3. Repeat for accel, brake, clutch (each should be a different ABS code).
4. Press **only** Cross. Write `code=` from `[raw]` into `btn_cross` in `config/g29_mapping.conf`. Re-run (no image rebuild; compose mounts that file). JSON `crossX` must become `true`.
5. Repeat for every button you care about. Fill the table:

| Control | conf key | raw `code=` | JSON field moved? |
| --- | --- | --- | --- |
| Steer | `steer_abs_code` | | `steeringAngleDeg` |
| Accel | `accel_abs_code` | | `acceleratorPedal` |
| Brake | `brake_abs_code` | | `brakePedal` |
| Clutch | `clutch_abs_code` | | `clutchPedal` |
| Cross | `btn_cross` | | `crossX` |
| Circle | `btn_circle` | | `circleO` |
| Square | `btn_square` | | `square` |
| Triangle | `btn_triangle` | | `triangle` |
| L paddle | `btn_left_paddle` | | `leftPaddleDownshift` |
| R paddle | `btn_right_paddle` | | `rightPaddleUpshift` |
| … | … | | … |

If raw shows a code and JSON still does not move after the edit: you are looking at a stale container config, or you edited the wrong key. Confirm the mount: compose maps `./config/g29_mapping.conf` → `/etc/cs_g29_reader/g29_mapping.conf`.

**D. Pedal invert / steering range (codes already right)**

- Released pedal JSON ~1 and floor ~0 → set `invert_pedals=0`
- Full lock not near `±(steering_range_deg/2)` → change `steering_range_deg`

**E. Re-check 500 Hz after mapping edits**

Same command as §6. Mapping changes do not affect rate; they only affect which JSON fields move.

### 6. 500 Hz

No JSON flood:

```bash
docker compose run --rm --device /dev/input:/dev/input g29-reader \
  --device auto --rate 500 --duration 5
```

**Pass:** `rate` near `500`, `blocked=0`, `mqtt_connected=false`. A few `overruns` is OK.

If `BLOCKED=DISCONNECTED`: permissions — `sudo ./build/g29_reader --device auto --rate 50` or `sudo usermod -aG input $USER` and log out.

Optional 2 s capture:

```bash
docker compose run --rm --device /dev/input:/dev/input g29-reader \
  --device auto --rate 500 --duration 2 --json-stdout \
  > /tmp/command.ndjson
wc -l /tmp/command.ndjson
head -n 1 /tmp/command.ndjson
```

### Stage 1 done when

- [ ] `046d:c24f`
- [ ] 500 Hz with `blocked=0` (mapping may still be wrong)
- [ ] `--dump-caps` run; `btn_*` values exist in the EV_KEY list
- [ ] worksheet: steer + pedals + at least Cross/paddles match JSON
- [ ] `g29_mapping.conf` saved with those codes
- [ ] photo or copy of one JSON line kept for Step 2

---

## Later (not written yet)

2. MQTT publish of the same JSON  
3. ROS 1 Noetic subscriber  
4. `feedback.json` + remaining interlocks

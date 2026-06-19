# HXLight BLE ADV for ESPHome

ESPHome external component for JOOFO/HXLight tunable-white floor lamps that are controlled by BLE advertising packets rather than BLE GATT, IR, or 433 MHz RF.

This component turns the lamp into a normal ESPHome/Home Assistant `light` entity with:

- on/off state
- brightness/level
- color temperature
- internal rolling sequence management
- no Home Assistant helpers, template lights, `python_script`, or raw-frame replay scripts

> Status: early component created from one known JOOFO/HXLight CCT floor lamp ([Wood Torchiere Floor Lamp](https://www.amazon.com/dp/B0GCFGC2ZX)). It should be treated as experimental until more owners test it.

## Requirements

- ESP32 board with Bluetooth
- ESPHome 2026.6.1 or newer (examples set `min_version: "2026.6.1"`)
- The lamp must use the HXLight/JOOFO BLE advertising protocol with packets beginning with `0201011bfff0ff` or `0201021bfff0ff`

ESPHome supports external components from local folders or Git repositories, and light platforms expose entities usable from Home Assistant. See the ESPHome external-components and light docs for the general mechanics.

## Minimal ESPHome YAML

```yaml
esphome:
  name: joofo-floor-lamp-proxy
  friendly_name: JOOFO Floor Lamp Proxy
  min_version: "2026.6.1"

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:
  level: DEBUG

api:
ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source: github://jaredhobbs/hxlight-ble-adv-esphome@main
    components: [hxlight_ble_adv]

hxlight_ble_adv:
  id: hxlight_controller
  adv_interval_min: 30ms
  adv_interval_max: 30ms
  adv_duration: 700ms
  adv_gap: 60ms
  max_queue_size: 32

light:
  - platform: hxlight_ble_adv
    id: floor_lamp
    name: "Floor Lamp"
    controller_id: hxlight_controller
    # No device_prefix: pair from the app with the button below.
    cold_white_color_temperature: 5000 K
    warm_white_color_temperature: 3000 K
    default_transition_length: 0s
    restore_mode: RESTORE_DEFAULT_OFF
    pair_sync:
      name: "Floor Lamp Pair/Sync"
    pair_sync_status:
      name: "Floor Lamp Sync Status"
```

After ESPHome adopts the device into Home Assistant, press **Floor Lamp Pair/Sync** and then press ON/OFF in the HXLight app within 30 seconds. The lamp learns its `device_prefix` and rolling sequence (persisted across reboots), after which `light.floor_lamp` supports brightness and color temperature directly. See [Pairing and resyncing](#pairing-and-resyncing).

## Configuration reference

### `hxlight_ble_adv:` controller

| Option | Required | Default | Notes |
|---|---:|---:|---|
| `id` | no | auto | Controller ID used by one or more lights. |
| `adv_interval_min` | no | `30ms` | BLE advertising interval lower bound. Increase if BLE/Wi-Fi is unstable. |
| `adv_interval_max` | no | `30ms` | BLE advertising interval upper bound. Keep equal to min for predictable behavior. |
| `adv_duration` | no | `700ms` | How long each generated command is advertised. Increase to `1200ms` if commands are missed. |
| `adv_gap` | no | `60ms` | Pause between queued commands. |
| `max_queue_size` | no | `32` | Drops commands if HA sends too many rapid changes. |
| `discovery` | no | `false` | Temporarily scan for HXLight Android app advertisements and log `device_prefix`/`initial_sequence` YAML values. Disable before normal lamp control. |

### `light:` platform

| Option | Required | Default | Notes |
|---|---:|---:|---|
| `platform` | yes | — | Must be `hxlight_ble_adv`. |
| `name` | yes | — | Home Assistant entity name. |
| `controller_id` | yes | — | ID of the `hxlight_ble_adv` controller. |
| `device_prefix` | no | learned | 8-byte lamp/app prefix. Omit it and pair from the app with the `pair_sync` button, or pin it explicitly (recommended for multi-lamp). A pinned prefix is never overwritten by pairing. |
| `initial_sequence` | no | `0` | Starting sequence value. Usually unnecessary now — pairing/resync sets it from the app. |
| `pair_sync` | no | — | Adds a button entity that arms pairing/resync for this light (learns prefix when unpaired, resyncs the sequence when paired). Sub-keys: `name`, `id`. |
| `pair_sync_status` | no | — | Adds a text-sensor showing live status: `Unpaired` / `Waiting for app (30s)` / `Paired (seq=N)` / `Synced (seq=N)` / `Timed out`. Sub-keys: `name`, `id`. |
| `restore_sequence` | no | `true` | Persist rolling sequence in ESPHome preferences. Keep enabled. |
| `cold_white_color_temperature` | no | `5000 K` | Lamp cold endpoint. Use your lamp's advertised spec. |
| `warm_white_color_temperature` | no | `3000 K` | Lamp warm endpoint. Use your lamp's advertised spec. |
| `command_duration` | no | controller default | Per-light override for `adv_duration`. |
| `command_gap` | no | controller default | Per-light override for `adv_gap`. |
| `flags` | no | `0x01` | BLE flags byte. Most captures use `0x01`; `0x02` also appears. |
| `send_on_with_state` | no | `true` | Sends ON when transitioning from off to on. |
| `send_brightness_on_turn_on` | no | `true` | Sends stored/current brightness after ON. |
| `send_color_temp_on_turn_on` | no | `true` | Sends stored/current CT after ON. |
| `default_transition_length` | no | ESPHome default | Strongly recommend `0s` to avoid many queued commands. |

## Pairing and resyncing

The `pair_sync` button is the easiest way to set up a lamp and to recover when the HXLight app has been used.

1. Press the light's **Pair/Sync** button in Home Assistant.
2. Within 30 seconds, press ON or OFF for that lamp in the HXLight Android/iOS app.
3. The ESP32 captures that app advertisement and:
   - **Unpaired light** (no `device_prefix`): learns the `device_prefix` and sets the sequence. Both are persisted, so it stays paired across reboots.
   - **Paired light**: resyncs the rolling sequence (use this whenever the app has advanced it and ESPHome control stopped working).

The `pair_sync_status` text-sensor shows progress: `Unpaired` → `Waiting for app (30s)` → `Paired (seq=N)` / `Synced (seq=N)`, or `Timed out` if no advertisement was captured.

Notes:
- The `device_prefix` is an 8-byte secret the app assigned when it paired with the lamp. It can only be observed from an app broadcast (the protocol is one-way and the value isn't enumerable), so the one app tap during pairing is required.
- A **pinned** `device_prefix` (set in YAML) is never overwritten — its button only resyncs the sequence and ignores advertisements from other lamps.
- Pair/Sync briefly scans and pauses transmission; it is unavailable while controller `discovery: true` is set.

### Home Assistant button with on-screen instructions

ESPHome can't open a dialog itself, but a Lovelace button card can show the instructions in a confirmation popup before arming:

```yaml
type: button
name: Pair / Sync Floor Lamp
icon: mdi:bluetooth-connect
tap_action:
  action: call-service
  service: button.press
  target:
    entity_id: button.floor_lamp_pair_sync
  confirmation:
    text: >-
      Press CONFIRM, then within 30 seconds press ON or OFF for this lamp in the
      HXLight app. Watch "Floor Lamp Sync Status" for the result.
```

Pair the card with an Entities card showing the `Floor Lamp Sync Status` sensor so you can see the live countdown/result.

## Discovering your `device_prefix` and `initial_sequence` (manual alternative)

Pairing with the `pair_sync` button (above) is the recommended path. Discovery mode remains available if you prefer to read the raw values from logs or pin them in YAML. Each lamp/app pairing has an 8-byte `device_prefix` and a rolling one-byte sequence; discovery mode scans for the Android HXLight app's BLE advertisements and prints the YAML values directly.

1. Flash a temporary config near the lamp with discovery enabled:

   ```yaml
   logger:
     level: DEBUG

   hxlight_ble_adv:
     id: hxlight_controller
     discovery: true
   ```

   `examples/discovery-temporary.yaml` is a complete temporary config.

2. Open ESPHome logs and press ON or OFF in the Android HXLight app. Discovery logs look like:

   ```text
   [I][hxlight_ble_adv:123]: Discovered HXLight packet from AA:BB:CC:DD:EE:FF RSSI=-62 command=on
   [I][hxlight_ble_adv:124]:   raw: 0201011bfff0ff6db643593a60a7a1632ce9b951fb2b4b198159daca90ff18
   [I][hxlight_ble_adv:125]:   device_prefix: "6db643593a60a7a1"
   [I][hxlight_ble_adv:126]:   observed_sequence: 0x6C / 108
   [I][hxlight_ble_adv:127]:   initial_sequence: 109
   [I][hxlight_ble_adv:128]: Copy the latest device_prefix and initial_sequence into YAML, set discovery: false, then recompile
   ```

3. Copy the latest printed values into your real light config, disable discovery, and recompile:

   ```yaml
   hxlight_ble_adv:
     id: hxlight_controller
     discovery: false

   light:
     - platform: hxlight_ble_adv
       name: "Floor Lamp"
       controller_id: hxlight_controller
       device_prefix: "6db643593a60a7a1"
       initial_sequence: 109
   ```

Use the last `initial_sequence` printed after your final Android app command. If the Android app sends multiple advertisements, each later line advances the rolling sequence.

### Manual fallback: parse a raw packet

If you cannot run discovery mode, capture Android Bluetooth HCI traffic while pressing ON/OFF in HXLight, then search for advertising payloads beginning with:

```text
0201011bfff0ff
0201021bfff0ff
```

The app-level Android logcat payload is useful for reverse engineering, but this component only needs the raw BLE advertisement. From the repo root, parse the raw packet:

```bash
python3 tools/parse_raw.py 0201011bfff0ff6db643593a60a7a1632ce9b951fb2b4b198159daca90ff18
```

Output:

```text
device_prefix: 6db643593a60a7a1
observed_sequence: 0x6c / 108
next_initial_sequence: 0x6d / 109
command_guess: on
```

Put those values into YAML:

```yaml
device_prefix: "6db643593a60a7a1"
initial_sequence: 109
```

If you use HXLight after flashing ESPHome, HXLight will advance the lamp sequence. Resync by enabling discovery again or parsing a fresh raw packet, then set `initial_sequence` to the latest next value and reflash. A future version should expose a runtime resync service/number.

## Adding another lamp

One ESP32 controller can manage multiple lamps if it is in Bluetooth range. Add another `light:` entry with a different `device_prefix` and `initial_sequence`:

```yaml
light:
  - platform: hxlight_ble_adv
    name: "Living Room Floor Lamp"
    controller_id: hxlight_controller
    device_prefix: "6db643593a60a7a1"
    initial_sequence: 109
    default_transition_length: 0s

  - platform: hxlight_ble_adv
    name: "Bedroom Floor Lamp"
    controller_id: hxlight_controller
    device_prefix: "0123456789abcdef"
    initial_sequence: 42
    default_transition_length: 0s
```

Each light owns its own restored sequence and Home Assistant state.

## Known protocol details

The raw packet is 31 bytes:

```text
02 01 <flags>
1b ff
f0 ff
<8-byte device prefix>
<11-byte command body>
<seq ^ b6>
<checksum ^ 2e>
<crc16_x25 ^ 6a4d, little-endian>
18
```

The command body is generated from the first 11 bytes of the HXLight app payload:

```text
command_body = payload11 XOR e5 1d fe b8 51 fa 2a b4 e7 d4 0c
```

Supported generated app payloads:

```text
ON:         86 31 17 01 00 01 01 ff fe 55 55 seq checksum
OFF:        86 31 17 01 00 01 02 ff fe 55 55 seq checksum
Brightness: 83 31 17 01 05 65 <01..64> fa 9a 55 55 seq checksum
CCT:        83 31 17 01 07 65 <cold> <warm> 9a 55 55 seq checksum
```

`checksum = sum(first 11 payload bytes + seq) & 0xff`.

## Troubleshooting

### Commands are missed

Increase duration:

```yaml
hxlight_ble_adv:
  adv_duration: 1200ms
```

or reduce Wi-Fi/BLE congestion by moving the ESP32 closer to the lamp.

### Brightness slider queues many commands

Set:

```yaml
default_transition_length: 0s
```

Avoid dragging the slider rapidly. HA may send many intermediate light states; every generated command consumes one sequence value and is queued.

### It worked, then stopped after using HXLight

Using the HXLight app advances the rolling sequence, so ESPHome falls out of sync. Press the lamp's **Pair/Sync** button and tap ON/OFF in the app within 30 seconds — the sequence resyncs with no reflash. See [Pairing and resyncing](#pairing-and-resyncing). (Add a `pair_sync` button to the light if you haven't yet.)

### Compile errors around Bluetooth advertising

Use a dedicated ESP32 for this bridge if possible. Avoid running other components that continuously own BLE advertising on the same device. BLE scanning/proxy features may work, but this component is designed to transmit raw advertisements and can conflict with other raw-advertising components.

### No raw packet begins with `0201011bfff0ff`

Your lamp may use a different HXLight protocol, iOS-style service UUID advertising, or a different app/controller. Open an issue with:

- lamp model/ASIN
- Android/iOS app name and version
- raw BLE logs around ON/OFF
- whether the lamp is CCT-only or RGB/RGBIC

## Development notes

This repo intentionally does not depend on `ble_adv_proxy`. It generates and advertises the BLE packets directly from ESPHome, and the controller's temporary `discovery: true` mode can learn the pairing values from HXLight Android app advertisements.

## License

MIT

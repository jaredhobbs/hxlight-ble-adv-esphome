# Changelog

## Unreleased

- Raised the default `adv_duration` from `700ms` to `2000ms`. Lamps that listen sparsely while idle/in standby frequently missed the shorter burst, so a command after a long off period would silently fail until retried; the longer window reaches them reliably. Override lower once delivery is solid if you want snappier multi-command transitions.
- Added controller `tx_power` (BLE transmit power, default `9` dBm / max on classic ESP32) and `prefer_ble` (default `true`, biases the ESP32's shared Wi-Fi/BLE radio toward BLE via the coexistence scheduler). Both improve command-advertisement reliability; pair `prefer_ble` with `wifi: { power_save_mode: none }` for best results.
- Added per-light **Pair/Sync**: a `pair_sync` button entity and optional `pair_sync_status` text-sensor. Press the button and tap the lamp in the HXLight app within 30s to learn the `device_prefix` (when unpaired) or resync the rolling sequence (when paired) — no reflash needed for app desync recovery.
- `device_prefix` is now **optional**. Omit it to pair from the app (learned prefix is persisted across reboots), or pin it in YAML (pinned prefixes are never overwritten and only resync the sequence). Unpaired lights ignore commands until paired.

- Fixed `ImportError: cannot import name 'request_bluetooth'` on ESPHome 2026.x. The component no longer calls the removed `esp32.request_bluetooth()`; it relies on the auto-loaded `esp32_ble` stack instead.
- Fixed an `ESPBTUUID has incomplete type` compile failure in `esp32_ble`. The component no longer defines `USE_ESP32_BLE_ADVERTISING` on its own (that macro is only valid paired with `USE_ESP32_BLE_UUID`). It transmits raw advertisements via the ESP-IDF GAP API and only needs the base `esp32_ble` stack, so no extra `esp32_ble` config is required.
- Set `min_version: "2026.6.1"` in all examples and documented ESPHome 2026.6.1 as the minimum supported version.
- Updated examples and README to use the platform-based `ota:` config (`ota: [{ platform: esphome }]`) required by recent ESPHome.
- Switched example ESP32 framework configs from Arduino to ESP-IDF.
- Added controller `discovery` mode to scan HXLight Android app advertisements and log YAML-ready `device_prefix` and `initial_sequence` values.

## 0.1.0

- Initial experimental ESPHome external component.
- Supports HXLight/JOOFO CCT BLE advertising lamps using generated packets.
- Supports on/off, brightness, and color temperature.
- Stores rolling sequence internally using ESPHome preferences.

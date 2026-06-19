# Changelog

## Unreleased

- Fixed `ImportError: cannot import name 'request_bluetooth'` on ESPHome 2026.x. The component no longer calls the removed `esp32.request_bluetooth()` and relies on `esp32_ble` instead. Configs must now include a top-level `esp32_ble: { advertising: true }` block (added to all examples).
- Switched example ESP32 framework configs from Arduino to ESP-IDF.
- Added controller `discovery` mode to scan HXLight Android app advertisements and log YAML-ready `device_prefix` and `initial_sequence` values.

## 0.1.0

- Initial experimental ESPHome external component.
- Supports HXLight/JOOFO CCT BLE advertising lamps using generated packets.
- Supports on/off, brightness, and color temperature.
- Stores rolling sequence internally using ESPHome preferences.

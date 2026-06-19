# Changelog

## Unreleased

- Switched example ESP32 framework configs from Arduino to ESP-IDF.
- Added controller `discovery` mode to scan HXLight Android app advertisements and log YAML-ready `device_prefix` and `initial_sequence` values.

## 0.1.0

- Initial experimental ESPHome external component.
- Supports HXLight/JOOFO CCT BLE advertising lamps using generated packets.
- Supports on/off, brightness, and color temperature.
- Stores rolling sequence internally using ESPHome preferences.

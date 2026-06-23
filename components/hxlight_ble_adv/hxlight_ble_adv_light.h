#pragma once

#include "hxlight_ble_adv.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#ifdef USE_ESP32
#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::text_sensor {
class TextSensor;
}  // namespace esphome::text_sensor

namespace esphome::hxlight_ble_adv {

// Persisted across reboots: the learned device prefix (if any), the rolling
// sequence, and whether a usable prefix has been paired.
struct HXLightPersist {
  uint8_t prefix[8];
  uint8_t sequence;
  uint8_t paired;
} __attribute__((packed));

class HXLightBLEAdvLight : public light::LightOutput, public Component {
 public:
  HXLightBLEAdvLight() = default;

  void setup() override;
  void dump_config() override;

  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

  void set_device_prefix(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                         uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
    this->device_prefix_ = {b0, b1, b2, b3, b4, b5, b6, b7};
    this->prefix_pinned_ = true;
    this->has_prefix_ = true;
  }
  void set_controller(HXLightBLEAdvController *controller) { this->controller_ = controller; }
  void set_preference_key(uint32_t key) { this->preference_key_ = key; }
  void set_initial_sequence(uint8_t seq) { this->sequence_ = seq; }
  void set_restore_sequence(bool restore) { this->restore_sequence_ = restore; }
  void set_cold_white_temperature(float mireds) { this->cold_white_temperature_ = mireds; }
  void set_warm_white_temperature(float mireds) { this->warm_white_temperature_ = mireds; }
  void set_command_duration(uint16_t ms) { this->command_duration_ms_ = ms; }
  void set_command_gap(uint16_t ms) { this->command_gap_ms_ = ms; }
  void set_flags(uint8_t flags) { this->flags_ = flags; }
  void set_send_on_with_state(bool val) { this->send_on_with_state_ = val; }
  void set_send_brightness_on_turn_on(bool val) { this->send_brightness_on_turn_on_ = val; }
  void set_send_color_temp_on_turn_on(bool val) { this->send_color_temp_on_turn_on_ = val; }
  void set_status_sensor(text_sensor::TextSensor *sensor) { this->status_sensor_ = sensor; }

  uint8_t get_sequence() const { return this->sequence_; }
  void set_sequence(uint8_t seq) {
    this->sequence_ = seq;
    this->persist_state_();
  }

  // Arms the controller to capture the next matching HXLight app advertisement
  // for this light, learning the device prefix (if not pinned) and resyncing
  // the rolling sequence.
  void request_pair_sync();

  // Called by the controller when a captured advertisement is offered to this
  // light. A pinned light only accepts a matching prefix; an unpaired light
  // learns the captured prefix. Returns true if the packet was claimed.
  bool try_apply_resync(const std::array<uint8_t, 8> &prefix, uint8_t next_sequence);

  void on_pair_sync_armed(uint32_t window_seconds);
  void on_pair_sync_timeout();

  bool has_prefix() const { return this->has_prefix_; }

 protected:
  static uint16_t crc16_x25_(const uint8_t *data, size_t len);
  uint8_t next_sequence_();
  void persist_state_();
  void publish_status_(const std::string &status);

  void send_on_();
  void send_off_();
  void send_brightness_(uint8_t level);
  void send_cct_(uint8_t cold, uint8_t warm);
  void send_payload11_(const std::array<uint8_t, 11> &payload11, HXAdvKind kind);

  std::array<uint8_t, 8> device_prefix_{};
  bool prefix_pinned_{false};
  bool has_prefix_{false};
  HXLightBLEAdvController *controller_{nullptr};

  uint8_t sequence_{0};
  bool restore_sequence_{true};
  uint32_t preference_key_{0};
  ESPPreferenceObject pref_{};

  text_sensor::TextSensor *status_sensor_{nullptr};

  float cold_white_temperature_{200.0f};  // 5000 K
  float warm_white_temperature_{333.333f};  // 3000 K
  uint16_t command_duration_ms_{0};
  uint16_t command_gap_ms_{0};
  uint8_t flags_{0x01};

  bool send_on_with_state_{true};
  bool send_brightness_on_turn_on_{false};
  bool send_color_temp_on_turn_on_{false};

  bool have_last_{false};
  bool last_on_{false};
  uint8_t last_brightness_{0};
  uint8_t last_cold_{255};
};

// Button entity that arms pair/sync for its parent light.
class HXLightPairSyncButton : public button::Button {
 public:
  void set_parent(HXLightBLEAdvLight *parent) { this->parent_ = parent; }

 protected:
  void press_action() override {
    if (this->parent_ != nullptr) this->parent_->request_pair_sync();
  }
  HXLightBLEAdvLight *parent_{nullptr};
};

}  // namespace esphome::hxlight_ble_adv
#endif  // USE_ESP32

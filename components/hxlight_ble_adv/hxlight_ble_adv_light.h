#pragma once

#include "hxlight_ble_adv.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#ifdef USE_ESP32
#include <array>
#include <cstddef>

namespace esphome::hxlight_ble_adv {

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
  }
  void set_controller(HXLightBLEAdvController *controller) { this->controller_ = controller; }
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

  uint8_t get_sequence() const { return this->sequence_; }
  void set_sequence(uint8_t seq) {
    this->sequence_ = seq;
    this->save_sequence_();
  }

 protected:
  enum CommandType : uint8_t { COMMAND_ON, COMMAND_OFF, COMMAND_BRIGHTNESS, COMMAND_CCT };

  static uint16_t crc16_x25_(const uint8_t *data, size_t len);
  uint32_t preference_hash_() const;
  uint8_t next_sequence_();
  void save_sequence_();

  void send_on_();
  void send_off_();
  void send_brightness_(uint8_t level);
  void send_cct_(uint8_t cold, uint8_t warm);
  void send_payload11_(const std::array<uint8_t, 11> &payload11, CommandType type);

  std::array<uint8_t, 8> device_prefix_{};
  HXLightBLEAdvController *controller_{nullptr};

  uint8_t sequence_{0};
  bool restore_sequence_{true};
  ESPPreferenceObject sequence_pref_{};

  float cold_white_temperature_{200.0f};  // 5000 K
  float warm_white_temperature_{333.333f};  // 3000 K
  uint16_t command_duration_ms_{0};
  uint16_t command_gap_ms_{0};
  uint8_t flags_{0x01};

  bool send_on_with_state_{true};
  bool send_brightness_on_turn_on_{true};
  bool send_color_temp_on_turn_on_{true};

  bool have_last_{false};
  bool last_on_{false};
  uint8_t last_brightness_{0};
  uint8_t last_cold_{255};
};

}  // namespace esphome::hxlight_ble_adv
#endif  // USE_ESP32

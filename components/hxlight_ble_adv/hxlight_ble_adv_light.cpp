#include "hxlight_ble_adv_light.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::hxlight_ble_adv {

static const char *const TAG = "hxlight_ble_adv.light";
static constexpr uint8_t BODY_XOR_MASK[11] = {0xE5, 0x1D, 0xFE, 0xB8, 0x51, 0xFA, 0x2A, 0xB4, 0xE7, 0xD4, 0x0C};

void HXLightBLEAdvLight::setup() {
  if (this->restore_sequence_) {
    this->sequence_pref_ = global_preferences->make_preference<uint8_t>(this->preference_hash_());
    uint8_t restored;
    if (this->sequence_pref_.load(&restored)) {
      this->sequence_ = restored;
      ESP_LOGI(TAG, "Restored HXLight sequence 0x%02X", this->sequence_);
    }
  }
}

void HXLightBLEAdvLight::dump_config() {
  char prefix[17];
  for (size_t i = 0; i < this->device_prefix_.size(); i++) {
    snprintf(prefix + (i * 2), 3, "%02x", this->device_prefix_[i]);
  }
  prefix[16] = '\0';

  ESP_LOGCONFIG(TAG, "HXLight BLE ADV Light:");
  ESP_LOGCONFIG(TAG, "  Device prefix: %s", prefix);
  ESP_LOGCONFIG(TAG, "  Sequence: 0x%02X", this->sequence_);
  ESP_LOGCONFIG(TAG, "  Restore sequence: %s", YESNO(this->restore_sequence_));
  ESP_LOGCONFIG(TAG, "  Cold white: %.1f mireds", this->cold_white_temperature_);
  ESP_LOGCONFIG(TAG, "  Warm white: %.1f mireds", this->warm_white_temperature_);
  ESP_LOGCONFIG(TAG, "  BLE flags byte: 0x%02X", this->flags_);
  if (this->controller_ == nullptr) {
    ESP_LOGE(TAG, "  No hxlight_ble_adv controller linked. Add controller_id to this light.");
  }
}

light::LightTraits HXLightBLEAdvLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
  traits.set_min_mireds(this->cold_white_temperature_);
  traits.set_max_mireds(this->warm_white_temperature_);
  return traits;
}

void HXLightBLEAdvLight::write_state(light::LightState *state) {
  if (this->controller_ == nullptr) {
    ESP_LOGE(TAG, "Cannot send HXLight command: no controller configured");
    return;
  }

  const bool is_on = state->current_values.is_on();

  float ct_ratio = 0.5f;
  float white_brightness = 1.0f;
  state->current_values_as_ct(&ct_ratio, &white_brightness);
  if (std::isnan(ct_ratio)) ct_ratio = 0.5f;
  if (std::isnan(white_brightness)) white_brightness = 1.0f;
  ct_ratio = clamp(ct_ratio, 0.0f, 1.0f);
  white_brightness = clamp(white_brightness, 0.0f, 1.0f);

  uint8_t brightness = static_cast<uint8_t>(roundf(white_brightness * 100.0f));
  if (brightness < 1) brightness = 1;
  if (brightness > 100) brightness = 100;

  uint8_t cold = static_cast<uint8_t>(roundf((1.0f - ct_ratio) * 100.0f));
  if (cold > 100) cold = 100;
  uint8_t warm = 100 - cold;

  if (!is_on) {
    if (!this->have_last_ || this->last_on_) {
      this->send_off_();
    }
    this->have_last_ = true;
    this->last_on_ = false;
    this->last_brightness_ = brightness;
    this->last_cold_ = cold;
    return;
  }

  const bool turning_on = !this->have_last_ || !this->last_on_;

  if (turning_on && this->send_on_with_state_) {
    this->send_on_();
  }

  if (!this->have_last_ || this->last_brightness_ != brightness ||
      (turning_on && this->send_brightness_on_turn_on_)) {
    this->send_brightness_(brightness);
  }

  if (!this->have_last_ || this->last_cold_ != cold ||
      (turning_on && this->send_color_temp_on_turn_on_)) {
    this->send_cct_(cold, warm);
  }

  this->have_last_ = true;
  this->last_on_ = true;
  this->last_brightness_ = brightness;
  this->last_cold_ = cold;
}

uint16_t HXLightBLEAdvLight::crc16_x25_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0x8408;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc ^ 0xFFFF;
}

uint32_t HXLightBLEAdvLight::preference_hash_() const {
  uint32_t hash = 2166136261UL;
  for (auto b : this->device_prefix_) {
    hash ^= b;
    hash *= 16777619UL;
  }
  return hash ^ 0x48584C54UL;  // "HXLT"
}

uint8_t HXLightBLEAdvLight::next_sequence_() {
  const uint8_t seq = this->sequence_;
  this->sequence_ = static_cast<uint8_t>(this->sequence_ + 1);
  this->save_sequence_();
  return seq;
}

void HXLightBLEAdvLight::save_sequence_() {
  if (this->restore_sequence_) {
    this->sequence_pref_.save(&this->sequence_);
  }
}

void HXLightBLEAdvLight::send_on_() {
  this->send_payload11_({0x86, 0x31, 0x17, 0x01, 0x00, 0x01, 0x01, 0xFF, 0xFE, 0x55, 0x55}, COMMAND_ON);
}

void HXLightBLEAdvLight::send_off_() {
  this->send_payload11_({0x86, 0x31, 0x17, 0x01, 0x00, 0x01, 0x02, 0xFF, 0xFE, 0x55, 0x55}, COMMAND_OFF);
}

void HXLightBLEAdvLight::send_brightness_(uint8_t level) {
  if (level < 1) level = 1;
  if (level > 100) level = 100;
  this->send_payload11_({0x83, 0x31, 0x17, 0x01, 0x05, 0x65, level, 0xFA, 0x9A, 0x55, 0x55}, COMMAND_BRIGHTNESS);
}

void HXLightBLEAdvLight::send_cct_(uint8_t cold, uint8_t warm) {
  if (cold > 100) cold = 100;
  if (warm > 100) warm = 100;
  this->send_payload11_({0x83, 0x31, 0x17, 0x01, 0x07, 0x65, cold, warm, 0x9A, 0x55, 0x55}, COMMAND_CCT);
}

void HXLightBLEAdvLight::send_payload11_(const std::array<uint8_t, 11> &payload11, CommandType type) {
  uint8_t seq = this->next_sequence_();
  uint8_t checksum = seq;
  for (auto b : payload11) checksum += b;

  std::array<uint8_t, 11> command_body{};
  for (size_t i = 0; i < command_body.size(); i++) {
    command_body[i] = payload11[i] ^ BODY_XOR_MASK[i];
  }

  const uint8_t tail0 = seq ^ 0xB6;
  const uint8_t tail1 = checksum ^ 0x2E;

  uint8_t crc_input[8 + 11 + 2];
  size_t pos = 0;
  for (auto b : this->device_prefix_) crc_input[pos++] = b;
  for (auto b : command_body) crc_input[pos++] = b;
  crc_input[pos++] = tail0;
  crc_input[pos++] = tail1;

  uint16_t crc = this->crc16_x25_(crc_input, sizeof(crc_input)) ^ 0x6A4D;

  std::array<uint8_t, 31> frame{};
  pos = 0;
  frame[pos++] = 0x02;
  frame[pos++] = 0x01;
  frame[pos++] = this->flags_;
  frame[pos++] = 0x1B;
  frame[pos++] = 0xFF;
  frame[pos++] = 0xF0;
  frame[pos++] = 0xFF;
  for (auto b : this->device_prefix_) frame[pos++] = b;
  for (auto b : command_body) frame[pos++] = b;
  frame[pos++] = tail0;
  frame[pos++] = tail1;
  frame[pos++] = crc & 0xFF;
  frame[pos++] = (crc >> 8) & 0xFF;
  frame[pos++] = 0x18;

  const char *type_s = "unknown";
  switch (type) {
    case COMMAND_ON: type_s = "on"; break;
    case COMMAND_OFF: type_s = "off"; break;
    case COMMAND_BRIGHTNESS: type_s = "brightness"; break;
    case COMMAND_CCT: type_s = "cct"; break;
  }

  ESP_LOGD(TAG, "Sending %s seq=0x%02X checksum=0x%02X", type_s, seq, checksum);
  this->controller_->enqueue(frame, this->command_duration_ms_, this->command_gap_ms_);
}

}  // namespace esphome::hxlight_ble_adv
#endif  // USE_ESP32

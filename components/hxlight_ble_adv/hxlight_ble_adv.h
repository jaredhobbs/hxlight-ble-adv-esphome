#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/esp32_ble/ble.h"

#ifdef USE_ESP32
#include <array>
#include <deque>
#include <vector>

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_gap_ble_api.h>
#include <esp_err.h>
#if __has_include(<esp_coexist.h>)
#include <esp_coexist.h>
#define HXLIGHT_HAS_COEXIST
#endif

namespace esphome::hxlight_ble_adv {

class HXLightBLEAdvLight;

struct HXLightAdvertisementTask {
  std::array<uint8_t, 31> data{};
  uint16_t duration_ms{1000};
  uint16_t gap_ms{60};
  uint8_t repeats{1};
};

class HXLightBLEAdvController : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_adv_interval_min(uint16_t val) { this->adv_interval_min_ms_ = val; }
  void set_adv_interval_max(uint16_t val) { this->adv_interval_max_ms_ = val; }
  void set_adv_duration(uint16_t val) { this->adv_duration_ms_ = val; }
  void set_adv_gap(uint16_t val) { this->adv_gap_ms_ = val; }
  void set_max_queue_size(uint8_t val) { this->max_queue_size_ = val; }
  void set_command_repeat(uint8_t val) { this->command_repeat_ = val; }
  void set_discovery(bool val) { this->discovery_enabled_ = val; }
  void set_tx_power(int8_t dbm) { this->tx_power_dbm_ = dbm; }
  void set_prefer_ble(bool val) { this->prefer_ble_ = val; }

  uint16_t get_default_duration() const { return this->adv_duration_ms_; }
  uint16_t get_default_gap() const { return this->adv_gap_ms_; }

  bool enqueue(const std::array<uint8_t, 31> &data, uint16_t duration_ms = 0, uint16_t gap_ms = 0);

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  void register_light(HXLightBLEAdvLight *light) { this->lights_.push_back(light); }

  // Arms a bounded scan that captures the next matching HXLight app
  // advertisement and offers it to the target light for pairing/resync.
  void arm_resync(HXLightBLEAdvLight *target, uint32_t window_seconds);

 protected:
  enum class State : uint8_t { IDLE, SETTING_DATA, STARTING, ADVERTISING, STOPPING };

  void start_next_();
  void begin_current_();
  void stop_current_();
  void finish_current_();
  void start_discovery_scan_();
  void handle_discovery_result_(const esp_ble_gap_cb_param_t *param);
  void end_resync_(bool success);
  bool scan_capture_active_() const { return this->discovery_enabled_ || this->resync_active_; }
  uint16_t interval_units_(uint16_t ms) const;
  void apply_radio_tuning_();
  static esp_power_level_t dbm_to_power_level_(int8_t dbm);

  uint16_t adv_interval_min_ms_{30};
  uint16_t adv_interval_max_ms_{30};
  uint16_t adv_duration_ms_{1000};
  uint16_t adv_gap_ms_{60};
  uint8_t max_queue_size_{32};
  uint8_t command_repeat_{3};
  bool discovery_enabled_{false};
  int8_t tx_power_dbm_{9};
  bool prefer_ble_{true};

  esp_ble_adv_params_t adv_params_{};
  esp_ble_scan_params_t scan_params_{};
  std::deque<HXLightAdvertisementTask> queue_{};
  HXLightAdvertisementTask current_{};
  State state_{State::IDLE};
  bool discovery_scan_param_pending_{false};
  bool discovery_scan_start_pending_{false};
  bool discovery_scanning_{false};
  bool have_last_discovery_packet_{false};
  std::array<uint8_t, 31> last_discovery_packet_{};

  std::vector<HXLightBLEAdvLight *> lights_{};
  bool resync_active_{false};
  uint32_t resync_window_s_{30};
  HXLightBLEAdvLight *resync_target_{nullptr};
};

}  // namespace esphome::hxlight_ble_adv
#endif  // USE_ESP32

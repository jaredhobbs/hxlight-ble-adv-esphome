#include "hxlight_ble_adv.h"
#include "hxlight_ble_adv_light.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace esphome::hxlight_ble_adv {

static const char *const TAG = "hxlight_ble_adv";
static constexpr uint32_t DISCOVERY_SCAN_DURATION_SECONDS = 30;
static constexpr uint8_t DISCOVERY_ON_BODY[11] = {0x63, 0x2C, 0xE9, 0xB9, 0x51, 0xFB,
                                                  0x2B, 0x4B, 0x19, 0x81, 0x59};
static constexpr uint8_t DISCOVERY_OFF_BODY[11] = {0x63, 0x2C, 0xE9, 0xB9, 0x51, 0xFB,
                                                   0x28, 0x4B, 0x19, 0x81, 0x59};
static constexpr uint8_t DISCOVERY_BRIGHTNESS_PREFIX[6] = {0x66, 0x2C, 0xE9, 0xB9, 0x54, 0x9F};
static constexpr uint8_t DISCOVERY_CCT_PREFIX_1[6] = {0x66, 0x2C, 0xE9, 0xB9, 0x51, 0xF4};
static constexpr uint8_t DISCOVERY_CCT_PREFIX_2[6] = {0x66, 0x2C, 0xE9, 0xB9, 0x56, 0x9F};

static uint16_t crc16_x25(const uint8_t *data, size_t len) {
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

static bool is_hxlight_discovery_packet(const uint8_t *data, uint8_t len) {
  if (len != 31) return false;
  if (data[0] != 0x02 || data[1] != 0x01) return false;
  if (data[2] != 0x01 && data[2] != 0x02) return false;
  if (data[3] != 0x1B || data[4] != 0xFF || data[5] != 0xF0 || data[6] != 0xFF) return false;
  if (data[30] != 0x18) return false;
  const uint16_t expected_crc = static_cast<uint16_t>(data[28]) | (static_cast<uint16_t>(data[29]) << 8);
  const uint16_t actual_crc = crc16_x25(data + 7, 21) ^ 0x6A4D;
  return expected_crc == actual_crc;
}

static void bytes_to_hex(const uint8_t *data, size_t len, char *out) {
  static constexpr char HEX[] = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = HEX[data[i] >> 4];
    out[i * 2 + 1] = HEX[data[i] & 0x0F];
  }
  out[len * 2] = '\0';
}

static const char *guess_command(const uint8_t *data, char *buffer, size_t len) {
  const uint8_t *body = data + 15;
  if (std::memcmp(body, DISCOVERY_ON_BODY, sizeof(DISCOVERY_ON_BODY)) == 0) return "on";
  if (std::memcmp(body, DISCOVERY_OFF_BODY, sizeof(DISCOVERY_OFF_BODY)) == 0) return "off";
  if (std::memcmp(body, DISCOVERY_BRIGHTNESS_PREFIX, sizeof(DISCOVERY_BRIGHTNESS_PREFIX)) == 0) {
    std::snprintf(buffer, len, "brightness %u%%", static_cast<unsigned>(data[21] ^ 0x2A));
    return buffer;
  }
  if (std::memcmp(body, DISCOVERY_CCT_PREFIX_1, sizeof(DISCOVERY_CCT_PREFIX_1)) == 0 ||
      std::memcmp(body, DISCOVERY_CCT_PREFIX_2, sizeof(DISCOVERY_CCT_PREFIX_2)) == 0) {
    std::snprintf(buffer, len, "color temperature cold=%u%%", static_cast<unsigned>(data[21] ^ 0x2A));
    return buffer;
  }
  return "unknown";
}

float HXLightBLEAdvController::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

uint16_t HXLightBLEAdvController::interval_units_(uint16_t ms) const {
  // BLE advertising intervals are in 0.625 ms units.
  const float units = static_cast<float>(ms) / 0.625f;
  if (units < 0x0020) return 0x0020;
  if (units > 0x4000) return 0x4000;
  return static_cast<uint16_t>(units);
}

void HXLightBLEAdvController::setup() {
  this->adv_params_ = {
      .adv_int_min = this->interval_units_(this->adv_interval_min_ms_),
      .adv_int_max = this->interval_units_(this->adv_interval_max_ms_),
      .adv_type = ADV_TYPE_NONCONN_IND,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .channel_map = ADV_CHNL_ALL,
      .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
  };

  this->apply_radio_tuning_();

  this->scan_params_ = {
      .scan_type = BLE_SCAN_TYPE_PASSIVE,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
      .scan_interval = 0x50,
      .scan_window = 0x50,
      .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
  };

  if (this->discovery_enabled_) {
    ESP_LOGI(TAG, "HXLight discovery enabled; use the Android app now and watch for YAML values below");
    this->set_timeout("hxlight_discovery_start", 1000, [this]() { this->start_discovery_scan_(); });
  }
}

void HXLightBLEAdvController::dump_config() {
  ESP_LOGCONFIG(TAG, "HXLight BLE ADV Controller:");
  ESP_LOGCONFIG(TAG, "  Adv interval: %ums - %ums", this->adv_interval_min_ms_, this->adv_interval_max_ms_);
  ESP_LOGCONFIG(TAG, "  Adv duration: %ums", this->adv_duration_ms_);
  ESP_LOGCONFIG(TAG, "  Adv gap: %ums", this->adv_gap_ms_);
  ESP_LOGCONFIG(TAG, "  Max queue size: %u", this->max_queue_size_);
  ESP_LOGCONFIG(TAG, "  TX power: %ddBm", static_cast<int>(this->tx_power_dbm_));
#ifdef HXLIGHT_HAS_COEXIST
  ESP_LOGCONFIG(TAG, "  Prefer BLE coexistence: %s", YESNO(this->prefer_ble_));
#endif
  ESP_LOGCONFIG(TAG, "  Discovery: %s", YESNO(this->discovery_enabled_));
}

esp_power_level_t HXLightBLEAdvController::dbm_to_power_level_(int8_t dbm) {
  switch (dbm) {
    case -12: return ESP_PWR_LVL_N12;
    case -9: return ESP_PWR_LVL_N9;
    case -6: return ESP_PWR_LVL_N6;
    case -3: return ESP_PWR_LVL_N3;
    case 0: return ESP_PWR_LVL_N0;
    case 3: return ESP_PWR_LVL_P3;
    case 6: return ESP_PWR_LVL_P6;
    case 9: return ESP_PWR_LVL_P9;
    default: return ESP_PWR_LVL_P9;
  }
}

void HXLightBLEAdvController::apply_radio_tuning_() {
  const esp_power_level_t level = dbm_to_power_level_(this->tx_power_dbm_);
  esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, level);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to set BLE adv TX power: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "BLE adv TX power set to %ddBm", static_cast<int>(this->tx_power_dbm_));
  }
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, level);

#ifdef HXLIGHT_HAS_COEXIST
  if (this->prefer_ble_) {
    esp_err_t cerr = esp_coex_preference_set(ESP_COEX_PREFER_BT);
    if (cerr != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set BLE coexistence preference: %s", esp_err_to_name(cerr));
    } else {
      ESP_LOGI(TAG, "BLE coexistence preference set to favor BT");
    }
  }
#endif
}

void HXLightBLEAdvController::start_discovery_scan_() {
  if (!this->scan_capture_active_() || this->discovery_scan_param_pending_ || this->discovery_scan_start_pending_ ||
      this->discovery_scanning_) {
    return;
  }

  this->discovery_scan_param_pending_ = true;
  esp_err_t err = esp_ble_gap_set_scan_params(&this->scan_params_);
  if (err != ESP_OK) {
    this->discovery_scan_param_pending_ = false;
    ESP_LOGW(TAG, "Failed to configure HXLight discovery scan: %s", esp_err_to_name(err));
    this->set_timeout("hxlight_discovery_retry", 5000, [this]() { this->start_discovery_scan_(); });
  }
}

void HXLightBLEAdvController::handle_discovery_result_(const esp_ble_gap_cb_param_t *param) {
  if (!this->scan_capture_active_() || param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) return;

  const uint8_t len = param->scan_rst.adv_data_len;
  const uint8_t *data = param->scan_rst.ble_adv;
  if (!is_hxlight_discovery_packet(data, len)) return;

  if (this->have_last_discovery_packet_ &&
      std::memcmp(this->last_discovery_packet_.data(), data, this->last_discovery_packet_.size()) == 0) {
    return;
  }
  std::memcpy(this->last_discovery_packet_.data(), data, this->last_discovery_packet_.size());
  this->have_last_discovery_packet_ = true;

  char raw[63];
  char prefix[17];
  char address[18];
  char command_buffer[40];
  bytes_to_hex(data, 31, raw);
  bytes_to_hex(data + 7, 8, prefix);
  std::snprintf(address, sizeof(address), "%02X:%02X:%02X:%02X:%02X:%02X",
                static_cast<unsigned>(param->scan_rst.bda[0]), static_cast<unsigned>(param->scan_rst.bda[1]),
                static_cast<unsigned>(param->scan_rst.bda[2]), static_cast<unsigned>(param->scan_rst.bda[3]),
                static_cast<unsigned>(param->scan_rst.bda[4]), static_cast<unsigned>(param->scan_rst.bda[5]));

  const uint8_t observed_sequence = data[26] ^ 0xB6;
  const uint8_t next_initial_sequence = static_cast<uint8_t>(observed_sequence + 1);
  const char *command = guess_command(data, command_buffer, sizeof(command_buffer));

  if (this->discovery_enabled_) {
    ESP_LOGI(TAG, "Discovered HXLight packet from %s RSSI=%d command=%s", address, param->scan_rst.rssi, command);
    ESP_LOGI(TAG, "  raw: %s", raw);
    ESP_LOGI(TAG, "  device_prefix: \"%s\"", prefix);
    ESP_LOGI(TAG, "  observed_sequence: 0x%02X / %u", static_cast<unsigned>(observed_sequence),
             static_cast<unsigned>(observed_sequence));
    ESP_LOGI(TAG, "  initial_sequence: %u", static_cast<unsigned>(next_initial_sequence));
    ESP_LOGI(TAG, "Copy the latest device_prefix and initial_sequence into YAML, set discovery: false, then recompile");
  }

  if (this->resync_active_ && this->resync_target_ != nullptr) {
    std::array<uint8_t, 8> prefix_bytes{};
    std::copy(data + 7, data + 15, prefix_bytes.begin());
    if (this->resync_target_->try_apply_resync(prefix_bytes, next_initial_sequence)) {
      this->end_resync_(true);
    }
  }
}

void HXLightBLEAdvController::arm_resync(HXLightBLEAdvLight *target, uint32_t window_seconds) {
  if (target == nullptr) return;
  if (this->resync_active_) {
    ESP_LOGW(TAG, "Pair/Sync already in progress; ignoring request");
    return;
  }
  if (this->discovery_enabled_) {
    ESP_LOGW(TAG, "Pair/Sync is unavailable while discovery mode is enabled");
    return;
  }

  this->resync_target_ = target;
  this->resync_window_s_ = window_seconds;
  this->resync_active_ = true;
  this->have_last_discovery_packet_ = false;  // ensure we capture a fresh packet
  ESP_LOGI(TAG, "Pair/Sync armed for %us; press ON/OFF in the HXLight app now",
           static_cast<unsigned>(window_seconds));
  target->on_pair_sync_armed(window_seconds);

  // Backstop in case the scan-complete event never arrives.
  this->set_timeout("hxlight_resync_timeout", (window_seconds * 1000) + 2000, [this]() { this->end_resync_(false); });
  this->start_discovery_scan_();
}

void HXLightBLEAdvController::end_resync_(bool success) {
  if (!this->resync_active_) return;
  this->resync_active_ = false;
  this->cancel_timeout("hxlight_resync_timeout");

  HXLightBLEAdvLight *target = this->resync_target_;
  this->resync_target_ = nullptr;

  if (this->discovery_scanning_ || this->discovery_scan_start_pending_) {
    esp_ble_gap_stop_scanning();
  }
  this->discovery_scanning_ = false;
  this->discovery_scan_start_pending_ = false;
  this->discovery_scan_param_pending_ = false;

  if (!success && target != nullptr) target->on_pair_sync_timeout();

  // Resume any commands queued while capture was in progress.
  this->start_next_();
}

bool HXLightBLEAdvController::enqueue(const std::array<uint8_t, 31> &data, uint16_t duration_ms, uint16_t gap_ms) {
  if (this->queue_.size() >= this->max_queue_size_) {
    ESP_LOGW(TAG, "Advertisement queue full; dropping HXLight command");
    return false;
  }

  HXLightAdvertisementTask task;
  task.data = data;
  task.duration_ms = duration_ms == 0 ? this->adv_duration_ms_ : duration_ms;
  task.gap_ms = gap_ms == 0 ? this->adv_gap_ms_ : gap_ms;
  this->queue_.push_back(task);
  ESP_LOGVV(TAG, "Queued HXLight raw advertisement; queue=%u", static_cast<unsigned>(this->queue_.size()));

  if (this->state_ == State::IDLE) this->start_next_();
  return true;
}

void HXLightBLEAdvController::start_next_() {
  // Pause transmission while capturing an app advertisement, to avoid GAP
  // scan/advertise collisions. Queued commands resume when resync ends.
  if (this->resync_active_) return;
  if (this->state_ != State::IDLE || this->queue_.empty()) return;

  this->current_ = this->queue_.front();
  this->queue_.pop_front();
  this->state_ = State::SETTING_DATA;

  ESP_LOGV(TAG, "Configuring HXLight advertisement for %ums", this->current_.duration_ms);
  esp_err_t err = esp_ble_gap_config_adv_data_raw(
      const_cast<uint8_t *>(this->current_.data.data()), this->current_.data.size());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(err));
    this->state_ = State::IDLE;
    this->set_timeout("hxlight_adv_retry", this->current_.gap_ms, [this]() { this->start_next_(); });
  }
}

void HXLightBLEAdvController::stop_current_() {
  if (this->state_ != State::ADVERTISING) return;
  this->state_ = State::STOPPING;
  esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gap_stop_advertising failed: %s", esp_err_to_name(err));
    this->finish_current_();
  }
}

void HXLightBLEAdvController::finish_current_() {
  this->state_ = State::IDLE;
  this->set_timeout("hxlight_adv_gap", this->current_.gap_ms, [this]() { this->start_next_(); });
}

void HXLightBLEAdvController::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  esp_err_t err;
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      if (!this->scan_capture_active_() || !this->discovery_scan_param_pending_) return;
      this->discovery_scan_param_pending_ = false;
      if (param->scan_param_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "HXLight discovery scan parameter setup failed: status=%u",
                 static_cast<unsigned>(param->scan_param_cmpl.status));
        this->set_timeout("hxlight_discovery_retry", 5000, [this]() { this->start_discovery_scan_(); });
        return;
      }
      err = esp_ble_gap_start_scanning(this->resync_active_ ? this->resync_window_s_
                                                            : DISCOVERY_SCAN_DURATION_SECONDS);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start HXLight discovery scan: %s", esp_err_to_name(err));
        this->set_timeout("hxlight_discovery_retry", 5000, [this]() { this->start_discovery_scan_(); });
      } else {
        this->discovery_scan_start_pending_ = true;
      }
      break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      if (!this->scan_capture_active_() || !this->discovery_scan_start_pending_) return;
      this->discovery_scan_start_pending_ = false;
      if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        this->discovery_scanning_ = false;
        ESP_LOGW(TAG, "HXLight discovery scan start failed: status=%u",
                 static_cast<unsigned>(param->scan_start_cmpl.status));
        this->set_timeout("hxlight_discovery_retry", 5000, [this]() { this->start_discovery_scan_(); });
        return;
      }
      this->discovery_scanning_ = true;
      ESP_LOGI(TAG, "HXLight discovery scanning for Android app advertisements");
      break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
      if (!this->scan_capture_active_()) return;
      if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
        if (this->discovery_scanning_) {
          this->discovery_scanning_ = false;
          if (this->resync_active_) {
            this->end_resync_(false);
          } else {
            this->set_timeout("hxlight_discovery_restart", 100, [this]() { this->start_discovery_scan_(); });
          }
        }
        return;
      }
      this->handle_discovery_result_(param);
      break;

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      if (!this->discovery_enabled_) return;
      if (!this->discovery_scanning_ && !this->discovery_scan_start_pending_) return;
      this->discovery_scan_start_pending_ = false;
      this->discovery_scanning_ = false;
      if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "HXLight discovery scan stop failed: status=%u",
                 static_cast<unsigned>(param->scan_stop_cmpl.status));
      }
      // Only discovery mode auto-restarts; resync stops are handled in end_resync_().
      this->set_timeout("hxlight_discovery_restart", 1000, [this]() { this->start_discovery_scan_(); });
      break;

    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      if (this->state_ != State::SETTING_DATA) return;
      this->state_ = State::STARTING;
      err = esp_ble_gap_start_advertising(&this->adv_params_);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
        this->finish_current_();
      }
      break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      if (this->state_ != State::STARTING) return;
      err = param->adv_start_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE adv start failed: %s", esp_err_to_name(err));
        this->finish_current_();
        return;
      }
      this->state_ = State::ADVERTISING;
      this->set_timeout("hxlight_adv_stop", this->current_.duration_ms, [this]() { this->stop_current_(); });
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (this->state_ != State::STOPPING) return;
      err = param->adv_stop_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "BLE adv stop failed: %s", esp_err_to_name(err));
      }
      this->finish_current_();
      break;

    default:
      break;
  }
}

}  // namespace esphome::hxlight_ble_adv
#endif  // USE_ESP32

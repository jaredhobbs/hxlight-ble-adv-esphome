"""ESPHome controller for HXLight/JOOFO BLE advertising lamps."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble
from esphome.components.esp32_ble import CONF_BLE_ID
from esphome.const import CONF_ID
from esphome.core import TimePeriod

AUTO_LOAD = ["esp32_ble"]
DEPENDENCIES = ["esp32"]

CONF_ADV_INTERVAL_MIN = "adv_interval_min"
CONF_ADV_INTERVAL_MAX = "adv_interval_max"
CONF_ADV_DURATION = "adv_duration"
CONF_ADV_GAP = "adv_gap"
CONF_MAX_QUEUE_SIZE = "max_queue_size"
CONF_DISCOVERY = "discovery"
CONF_TX_POWER = "tx_power"
CONF_PREFER_BLE = "prefer_ble"

hxlight_ble_adv_ns = cg.esphome_ns.namespace("hxlight_ble_adv")
HXLightBLEAdvController = hxlight_ble_adv_ns.class_("HXLightBLEAdvController", cg.Component)


def _validate_intervals(config):
    if config[CONF_ADV_INTERVAL_MIN] > config[CONF_ADV_INTERVAL_MAX]:
        raise cv.Invalid("adv_interval_min must be <= adv_interval_max")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HXLightBLEAdvController),
            cv.GenerateID(CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
            cv.Optional(CONF_ADV_INTERVAL_MIN, default="30ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=TimePeriod(milliseconds=20), max=TimePeriod(milliseconds=10240)),
            ),
            cv.Optional(CONF_ADV_INTERVAL_MAX, default="30ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=TimePeriod(milliseconds=20), max=TimePeriod(milliseconds=10240)),
            ),
            cv.Optional(CONF_ADV_DURATION, default="700ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=TimePeriod(milliseconds=50), max=TimePeriod(milliseconds=10000)),
            ),
            cv.Optional(CONF_ADV_GAP, default="60ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=TimePeriod(milliseconds=0), max=TimePeriod(milliseconds=5000)),
            ),
            cv.Optional(CONF_MAX_QUEUE_SIZE, default=32): cv.int_range(min=1, max=255),
            cv.Optional(CONF_TX_POWER, default=9): cv.one_of(-12, -9, -6, -3, 0, 3, 6, 9, int=True),
            cv.Optional(CONF_PREFER_BLE, default=True): cv.boolean,
            cv.Optional(CONF_DISCOVERY, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_intervals,
)

FINAL_VALIDATE_SCHEMA = esp32_ble.validate_variant


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_BLE_ID])
    esp32_ble.register_gap_event_handler(parent, var)
    await cg.register_component(var, config)

    cg.add(var.set_adv_interval_min(config[CONF_ADV_INTERVAL_MIN]))
    cg.add(var.set_adv_interval_max(config[CONF_ADV_INTERVAL_MAX]))
    cg.add(var.set_adv_duration(config[CONF_ADV_DURATION]))
    cg.add(var.set_adv_gap(config[CONF_ADV_GAP]))
    cg.add(var.set_max_queue_size(config[CONF_MAX_QUEUE_SIZE]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_prefer_ble(config[CONF_PREFER_BLE]))
    cg.add(var.set_discovery(config[CONF_DISCOVERY]))

    # This component transmits raw BLE advertisements directly through the
    # ESP-IDF GAP API (esp_ble_gap_*), so it only needs the base esp32_ble
    # stack initialized (USE_ESP32_BLE, provided via AUTO_LOAD). It does not
    # use esp32_ble's BLEAdvertising helper, so it must NOT define
    # USE_ESP32_BLE_ADVERTISING on its own: that macro is only valid when
    # paired with USE_ESP32_BLE_UUID, which esp32_ble defines together when
    # `advertising: true` is set. Defining it alone breaks the esp32_ble build
    # (ESPBTUUID becomes an incomplete type). ESPHome 2026.x also no longer
    # exposes esp32.request_bluetooth(); esp32_ble handles BLE initialization.

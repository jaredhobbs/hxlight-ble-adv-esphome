"""Light platform for HXLight/JOOFO BLE advertising lamps."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_ID,
    CONF_OUTPUT_ID,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

from . import HXLightBLEAdvController, hxlight_ble_adv_ns

AUTO_LOAD = ["light"]
DEPENDENCIES = ["hxlight_ble_adv"]

CONF_CONTROLLER_ID = "controller_id"
CONF_DEVICE_PREFIX = "device_prefix"
CONF_INITIAL_SEQUENCE = "initial_sequence"
CONF_RESTORE_SEQUENCE = "restore_sequence"
CONF_COMMAND_DURATION = "command_duration"
CONF_COMMAND_GAP = "command_gap"
CONF_FLAGS = "flags"
CONF_SEND_ON_WITH_STATE = "send_on_with_state"
CONF_SEND_BRIGHTNESS_ON_TURN_ON = "send_brightness_on_turn_on"
CONF_SEND_COLOR_TEMP_ON_TURN_ON = "send_color_temp_on_turn_on"

HXLightBLEAdvLight = hxlight_ble_adv_ns.class_(
    "HXLightBLEAdvLight", light.LightOutput, cg.Component
)


def _hex_bytes_exact(length):
    def validator(value):
        value = str(value).replace(" ", "").replace(":", "").replace("-", "").lower()
        if len(value) != length * 2:
            raise cv.Invalid(f"must be exactly {length} bytes / {length * 2} hex characters")
        try:
            bytes.fromhex(value)
        except ValueError as err:
            raise cv.Invalid("must be valid hex") from err
        return value

    return validator


def _validate_config(config):
    # Reuse ESPHome's standard color-temperature validation for cold < warm.
    return light.validate_color_temperature_channels(config)


CONFIG_SCHEMA = cv.All(
    light.RGB_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HXLightBLEAdvLight),
            cv.Required(CONF_CONTROLLER_ID): cv.use_id(HXLightBLEAdvController),
            cv.Required(CONF_DEVICE_PREFIX): _hex_bytes_exact(8),
            cv.Optional(CONF_INITIAL_SEQUENCE, default=0): cv.int_range(min=0, max=255),
            cv.Optional(CONF_RESTORE_SEQUENCE, default=True): cv.boolean,
            cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, default="5000 K"): cv.color_temperature,
            cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, default="3000 K"): cv.color_temperature,
            cv.Optional(CONF_COMMAND_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_COMMAND_GAP): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_FLAGS, default=0x01): cv.int_range(min=0, max=255),
            cv.Optional(CONF_SEND_ON_WITH_STATE, default=True): cv.boolean,
            cv.Optional(CONF_SEND_BRIGHTNESS_ON_TURN_ON, default=True): cv.boolean,
            cv.Optional(CONF_SEND_COLOR_TEMP_ON_TURN_ON, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_config,
)


async def to_code(config):
    prefix = config[CONF_DEVICE_PREFIX]
    prefix_arr = [cg.RawExpression(f"0x{prefix[i:i + 2]}") for i in range(0, len(prefix), 2)]

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    cg.add(var.set_device_prefix(*prefix_arr))
    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    cg.add(var.set_controller(controller))

    cg.add(var.set_initial_sequence(config[CONF_INITIAL_SEQUENCE]))
    cg.add(var.set_restore_sequence(config[CONF_RESTORE_SEQUENCE]))
    cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
    if CONF_COMMAND_DURATION in config:
        cg.add(var.set_command_duration(config[CONF_COMMAND_DURATION]))
    if CONF_COMMAND_GAP in config:
        cg.add(var.set_command_gap(config[CONF_COMMAND_GAP]))
    cg.add(var.set_flags(config[CONF_FLAGS]))
    cg.add(var.set_send_on_with_state(config[CONF_SEND_ON_WITH_STATE]))
    cg.add(var.set_send_brightness_on_turn_on(config[CONF_SEND_BRIGHTNESS_ON_TURN_ON]))
    cg.add(var.set_send_color_temp_on_turn_on(config[CONF_SEND_COLOR_TEMP_ON_TURN_ON]))

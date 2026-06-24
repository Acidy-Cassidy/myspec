"""
ESPectre Component — MVS WiFi CSI Motion Detection
"""

from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor, number, switch
from esphome.components.esp32 import add_extra_build_file, add_idf_sdkconfig_option

try:
    from esphome.components.esp32 import include_builtin_idf_component
except ImportError:
    include_builtin_idf_component = None

from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_MOTION,
    UNIT_EMPTY,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)

DEPENDENCIES = ["wifi"]
AUTO_LOAD = ["sensor", "binary_sensor", "number", "switch"]

CONF_SEGMENTATION_THRESHOLD = "segmentation_threshold"
CONF_SEGMENTATION_WINDOW_SIZE = "segmentation_window_size"
CONF_PUBLISH_INTERVAL = "publish_interval"
CONF_EVALUATION_INTERVAL = "evaluation_interval"
CONF_MOTION_ON_HITS = "motion_on_hits"
CONF_MOTION_OFF_HITS = "motion_off_hits"
CONF_SELECTED_SUBCARRIERS = "selected_subcarriers"
CONF_LOWPASS_ENABLED = "lowpass_enabled"
CONF_LOWPASS_CUTOFF = "lowpass_cutoff"
CONF_HAMPEL_ENABLED = "hampel_enabled"
CONF_HAMPEL_WINDOW = "hampel_window"
CONF_HAMPEL_THRESHOLD = "hampel_threshold"
CONF_GAIN_LOCK = "gain_lock"
CONF_MOVEMENT_SENSOR = "movement_sensor"
CONF_MOTION_SENSOR = "motion_sensor"
CONF_THRESHOLD_NUMBER = "threshold_number"
CONF_CALIBRATE_SWITCH = "calibrate_switch"

THRESHOLD_MIN = 0.0
THRESHOLD_MAX = 10.0

espectre_ns = cg.esphome_ns.namespace("espectre")
ESpectreComponent = espectre_ns.class_("ESpectreComponent", cg.Component)
ESpectreThresholdNumber = espectre_ns.class_("ESpectreThresholdNumber", number.Number, cg.Component)
ESpectreCalibrateSwitch = espectre_ns.class_("ESpectreCalibrateSwitch", switch.Switch, cg.Component)


def validate_segmentation_threshold(value):
    if isinstance(value, str):
        value_lower = value.lower()
        if value_lower in ("auto", "min"):
            return value_lower
        try:
            return float(value)
        except ValueError:
            raise cv.Invalid(f"Invalid threshold '{value}'. Use 'auto', 'min', or a number 0-10")
    if isinstance(value, (int, float)):
        if value < THRESHOLD_MIN or value > THRESHOLD_MAX:
            raise cv.Invalid(f"Threshold must be between {THRESHOLD_MIN} and {THRESHOLD_MAX}")
        return float(value)
    raise cv.Invalid("Invalid threshold type. Use 'auto', 'min', or a number 0-10")


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESpectreComponent),
    cv.Optional(CONF_SEGMENTATION_THRESHOLD, default="auto"): validate_segmentation_threshold,
    cv.Optional(CONF_SEGMENTATION_WINDOW_SIZE, default=100): cv.int_range(min=10, max=200),
    cv.Optional(CONF_GAIN_LOCK, default="auto"): cv.one_of("auto", "enabled", "disabled", lower=True),
    cv.Optional(CONF_PUBLISH_INTERVAL, default=100): cv.int_range(min=1, max=1000),
    cv.Optional(CONF_EVALUATION_INTERVAL, default=25): cv.int_range(min=1, max=1000),
    cv.Optional(CONF_MOTION_ON_HITS, default=3): cv.int_range(min=1, max=20),
    cv.Optional(CONF_MOTION_OFF_HITS, default=3): cv.int_range(min=1, max=20),
    cv.Optional(CONF_SELECTED_SUBCARRIERS): cv.All(
        cv.ensure_list(cv.int_range(min=0, max=63)),
        cv.Length(min=1, max=12)
    ),
    cv.Optional(CONF_LOWPASS_ENABLED, default=False): cv.boolean,
    cv.Optional(CONF_LOWPASS_CUTOFF, default=11.0): cv.float_range(min=5.0, max=20.0),
    cv.Optional(CONF_HAMPEL_ENABLED, default=True): cv.boolean,
    cv.Optional(CONF_HAMPEL_WINDOW, default=7): cv.int_range(min=3, max=11),
    cv.Optional(CONF_HAMPEL_THRESHOLD, default=5.0): cv.float_range(min=1.0, max=10.0),
    cv.Optional(CONF_MOVEMENT_SENSOR, default={"name": "Movement Score"}): sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MOTION_SENSOR, default={"name": "Motion Detected"}): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION,
    ),
    cv.Optional(CONF_THRESHOLD_NUMBER, default={"name": "Threshold"}): number.number_schema(
        ESpectreThresholdNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_PULSE,
    ),
    cv.Optional(CONF_CALIBRATE_SWITCH, default={"name": "Calibrate"}): switch.switch_schema(
        ESpectreCalibrateSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}).extend(cv.COMPONENT_SCHEMA)



async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    partitions_path = Path(__file__).parent / "partitions.csv"
    if partitions_path.exists():
        add_extra_build_file("partitions.csv", partitions_path)
        cg.add_platformio_option("board_build.partitions", "partitions.csv")

    if include_builtin_idf_component is not None:
        include_builtin_idf_component("spiffs")

    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_CSI_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_PM_ENABLE", False)
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE", False)
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_TX_ENABLED", False)
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_RX_ENABLED", False)
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM", 128)

    threshold_value = config[CONF_SEGMENTATION_THRESHOLD]
    if isinstance(threshold_value, str):
        cg.add(var.set_threshold_mode(threshold_value))
    else:
        cg.add(var.set_segmentation_threshold(threshold_value))

    cg.add(var.set_segmentation_window_size(config[CONF_SEGMENTATION_WINDOW_SIZE]))
    cg.add(var.set_gain_lock_mode(config[CONF_GAIN_LOCK]))
    cg.add(var.set_publish_interval(config[CONF_PUBLISH_INTERVAL]))
    cg.add(var.set_evaluation_interval(config[CONF_EVALUATION_INTERVAL]))
    cg.add(var.set_motion_on_hits(config[CONF_MOTION_ON_HITS]))
    cg.add(var.set_motion_off_hits(config[CONF_MOTION_OFF_HITS]))

    if CONF_SELECTED_SUBCARRIERS in config:
        cg.add(var.set_selected_subcarriers(config[CONF_SELECTED_SUBCARRIERS]))

    cg.add(var.set_lowpass_enabled(config[CONF_LOWPASS_ENABLED]))
    cg.add(var.set_lowpass_cutoff(config[CONF_LOWPASS_CUTOFF]))
    cg.add(var.set_hampel_enabled(config[CONF_HAMPEL_ENABLED]))
    cg.add(var.set_hampel_window(config[CONF_HAMPEL_WINDOW]))
    cg.add(var.set_hampel_threshold(config[CONF_HAMPEL_THRESHOLD]))

    sens = await sensor.new_sensor(config[CONF_MOVEMENT_SENSOR])
    cg.add(var.set_movement_sensor(sens))

    sens = await binary_sensor.new_binary_sensor(config[CONF_MOTION_SENSOR])
    cg.add(var.set_motion_binary_sensor(sens))

    num = await number.new_number(
        config[CONF_THRESHOLD_NUMBER],
        min_value=THRESHOLD_MIN,
        max_value=THRESHOLD_MAX,
        step=0.001,
    )
    cg.add(num.set_parent(var))
    cg.add(var.set_threshold_number(num))

    sw = await switch.new_switch(config[CONF_CALIBRATE_SWITCH])
    cg.add(sw.set_parent(var))
    cg.add(var.set_calibrate_switch(sw))

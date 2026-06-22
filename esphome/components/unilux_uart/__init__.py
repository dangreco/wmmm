import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from esphome.components import sensor, uart

CODEOWNERS = ["@dangreco"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

CONF_T1 = "t1"
CONF_T2 = "t2"
CONF_DISABLED = "disabled"

CHANNELS = (CONF_T1, CONF_T2)

unilux_uart_ns = cg.esphome_ns.namespace("unilux_uart")
UniluxUartComponent = unilux_uart_ns.class_(
    "UniluxUartComponent", cg.Component, uart.UARTDevice
)


def _channel_schema(key):
    """Sensor schema for one temperature channel.

    The channel defaults to enabled with its key as the sensor name, so omitting
    the key still produces a published sensor. Set ``disabled: true`` to suppress
    the channel entirely.
    """
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.Optional(CONF_NAME, default=key): cv.string,
            cv.Optional(CONF_DISABLED, default=False): cv.boolean,
        }
    )


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UniluxUartComponent),
            cv.Optional(CONF_T1, default={}): _channel_schema(CONF_T1),
            cv.Optional(CONF_T2, default={}): _channel_schema(CONF_T2),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for key in CHANNELS:
        conf = config[key]
        if conf[CONF_DISABLED]:
            continue
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var, f"set_{key}_sensor")(sens))

    cg.add_library("unilux-uart", "main", "https://github.com/dangreco/unilux-uart.git")

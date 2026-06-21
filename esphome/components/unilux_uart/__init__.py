import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)

from esphome.components import uart

CODEOWNERS = ["@dangreco"]
DEPENDENCIES = ["uart"]

unilux_uart_ns = cg.esphome_ns.namespace("unilux_uart")
UniluxUartComponent = unilux_uart_ns.class_(
    "UniluxUartComponent", cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UniluxUartComponent),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add_library("unilux-uart", "main", "https://github.com/dangreco/unilux-uart.git")

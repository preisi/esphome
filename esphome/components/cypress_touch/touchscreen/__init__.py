import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import i2c, touchscreen
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN

CODEOWNERS = ["@preisi"]
DEPENDENCIES = ["i2c"]

cypress_touch_ns = cg.esphome_ns.namespace("cypress_touch")
CypressTouchscreen = cypress_touch_ns.class_(
    "CypressTouchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONF_CYPRESS_TOUCH_ID = "cypress_touch_id"
CONF_RTS_PIN = "rts_pin"
CONF_ENABLE_PIN = "enable_pin"

CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CypressTouchscreen),
            cv.Required(CONF_INTERRUPT_PIN): cv.All(
                pins.internal_gpio_input_pin_schema
            ),
            cv.Required(CONF_RTS_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
        }
    ).extend(i2c.i2c_device_schema(0x24))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    cg.add(var.set_interrupt_pin(interrupt_pin))
    rts_pin = await cg.gpio_pin_expression(config[CONF_RTS_PIN])
    cg.add(var.set_rts_pin(rts_pin))
    enable_pin = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable_pin(enable_pin))
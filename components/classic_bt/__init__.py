import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32

from esphome.const import CONF_ID

bluetooth_ns = cg.esphome_ns.namespace("classic_bt_component")
ClassicBluetooth = bluetooth_ns.class_("ClassicBluetooth", cg.Component)

CONF_ADDRESS = "address"
CONF_AUTO_ACCEPT_PAIRING = "auto_accept_pairing"
CONF_DEBUG_LOGGING = "debug_logging"
CONF_ENABLED = "enabled"
CONF_LOCAL_NAME = "local_name"


def validate_address(value):
    address = cv.string(value)
    octets = address.split(":")
    if len(octets) != 6 or any(
        len(octet) != 2
        or any(character not in "0123456789abcdefABCDEF" for character in octet)
        for octet in octets
    ):
        raise cv.Invalid("address must use the format XX:XX:XX:XX:XX:XX")
    return address


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ClassicBluetooth),
        cv.Required(CONF_ADDRESS): validate_address,
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_LOCAL_NAME, default="ESP32-Classic-Bluetooth"): cv.string,
        cv.Optional(CONF_AUTO_ACCEPT_PAIRING, default=True): cv.boolean,
        cv.Optional(CONF_DEBUG_LOGGING, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    esp32.request_bluetooth()

    cg.add_library("BluetoothSerial", None)

    cg.add_cxx_build_flag(
        "-I${IDF_PATH}/components/bt/host/bluedroid/api/include/api"
    )
    cg.add_cxx_build_flag(
        "-I${IDF_PATH}/components/bt/host/bluedroid/api/include"
    )

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    cg.add(var.set_local_name(config[CONF_LOCAL_NAME]))
    cg.add(var.set_auto_accept_pairing(config[CONF_AUTO_ACCEPT_PAIRING]))
    cg.add(var.set_debug_logging(config[CONF_DEBUG_LOGGING]))

    await cg.register_component(var, config)

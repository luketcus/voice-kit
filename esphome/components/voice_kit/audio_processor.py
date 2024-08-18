import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID

from . import voice_kit_ns, VoiceKit

CODEOWNERS = ["@kbx81"]

CONF_VNR_VALUE = "vnr_value"

SetVnrValueAction = voice_kit_ns.class_("SetVnrValueAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(VoiceKit),
        cv.Optional(CONF_VNR_VALUE): cv.uint8_t,
    }
)

SET_VNR_VALUE_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(VoiceKit),
        cv.Required(CONF_VNR_VALUE): cv.templatable(cv.uint8_t),
    },
    key=CONF_VNR_VALUE,
)


@automation.register_action(
    "voice_kit.set_vnr_value", SetVnrValueAction, SET_VNR_VALUE_ACTION_SCHEMA
)
async def voice_kit_set_vnr_value_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config.get(CONF_VNR_VALUE), args, int)
    cg.add(var.set_vnr_value(template_))

    return var


async def to_code(config):
    var = await cg.get_variable(config[CONF_ID])

    if vnr_value := config.get(CONF_VNR_VALUE) is not None:
        cg.add(var.set_vnr_value(vnr_value))

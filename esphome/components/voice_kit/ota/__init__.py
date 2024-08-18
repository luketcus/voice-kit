import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_PASSWORD,
    CONF_URL,
    CONF_USERNAME,
)
from esphome.components.http_request import (
    CONF_HTTP_REQUEST_ID,
    HttpRequestComponent,
)
from esphome.components.ota import BASE_OTA_SCHEMA, ota_to_code, OTAComponent
from esphome.core import coroutine_with_priority
from .. import voice_kit_ns, VoiceKit

CODEOWNERS = ["@kbx81"]

AUTO_LOAD = ["md5"]
DEPENDENCIES = ["network", "http_request", "voice_kit"]

CONF_MD5 = "md5"
CONF_MD5_URL = "md5_url"
CONF_VOICE_KIT_ID = "voice_kit_id"

OtaVoiceKitComponent = voice_kit_ns.class_("OtaVoiceKitComponent", OTAComponent)
OtaVoiceKitComponentFlashAction = voice_kit_ns.class_(
    "OtaVoiceKitComponentFlashAction", automation.Action
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OtaVoiceKitComponent),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.GenerateID(CONF_VOICE_KIT_ID): cv.use_id(VoiceKit),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)


@coroutine_with_priority(52.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_HTTP_REQUEST_ID])

    vk_paren = await cg.get_variable(config[CONF_VOICE_KIT_ID])
    cg.add(var.set_voice_kit(vk_paren))


OTA_VOICE_KIT_FLASH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(OtaVoiceKitComponent),
            cv.Optional(CONF_MD5_URL): cv.templatable(cv.url),
            cv.Optional(CONF_MD5): cv.templatable(
                cv.All(cv.string, cv.Length(min=32, max=32))
            ),
            cv.Optional(CONF_PASSWORD): cv.templatable(cv.string),
            cv.Optional(CONF_USERNAME): cv.templatable(cv.string),
            cv.Required(CONF_URL): cv.templatable(cv.url),
        }
    ),
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_URL),
)


@automation.register_action(
    "ota.voice_kit.flash",
    OtaVoiceKitComponentFlashAction,
    OTA_VOICE_KIT_FLASH_ACTION_SCHEMA,
)
async def ota_voice_kit_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if md5_url := config.get(CONF_MD5_URL):
        template_ = await cg.templatable(md5_url, args, cg.std_string)
        cg.add(var.set_md5_url(template_))

    if md5_str := config.get(CONF_MD5):
        template_ = await cg.templatable(md5_str, args, cg.std_string)
        cg.add(var.set_md5(template_))

    if password_str := config.get(CONF_PASSWORD):
        template_ = await cg.templatable(password_str, args, cg.std_string)
        cg.add(var.set_password(template_))

    if username_str := config.get(CONF_USERNAME):
        template_ = await cg.templatable(username_str, args, cg.std_string)
        cg.add(var.set_username(template_))

    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))

    return var

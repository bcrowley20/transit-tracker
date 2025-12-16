import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, font, time as time_, image
from esphome.components.http_request import CONF_HTTP_REQUEST_ID, HttpRequestComponent
from esphome.const import CONF_ID, CONF_DISPLAY_ID, CONF_TIME_ID

DEPENDENCIES = ["network", "http_request"]
AUTO_LOAD = ["json"]

soccer_tracker_ns = cg.esphome_ns.namespace("soccer_tracker")
SoccerTracker = soccer_tracker_ns.class_("SoccerTracker", cg.Component)

CONF_FONT_ID = "font_id"
CONF_SMALL_FONT_ID = "small_font_id"
CONF_API_KEY = "api_key"
CONF_FAVORITE_TEAM = "favorite_team"
CONF_TEAM_ID = "team_id"
CONF_TEAM_LOGOS = "team_logos"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SoccerTracker),
        cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(display.Display),
        cv.GenerateID(CONF_FONT_ID): cv.use_id(font.Font),
        cv.GenerateID(CONF_SMALL_FONT_ID): cv.use_id(font.Font),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
        cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
        cv.Required(CONF_API_KEY): cv.string,
        cv.Required(CONF_FAVORITE_TEAM): cv.string,
        cv.Required(CONF_TEAM_ID): cv.positive_int,
        cv.Optional(CONF_TEAM_LOGOS, default={}): cv.Schema({
            cv.string: cv.use_id(image.Image_)
        }),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(disp))

    font_var = await cg.get_variable(config[CONF_FONT_ID])
    cg.add(var.set_font(font_var))

    small_font_var = await cg.get_variable(config[CONF_SMALL_FONT_ID])
    cg.add(var.set_small_font(small_font_var))

    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_rtc(time_var))

    http_request_var = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
    cg.add(var.set_http_request(http_request_var))

    cg.add(var.set_api_key(config[CONF_API_KEY]))
    cg.add(var.set_favorite_team(config[CONF_FAVORITE_TEAM]))
    cg.add(var.set_team_id(config[CONF_TEAM_ID]))

    # Register team logos
    if CONF_TEAM_LOGOS in config:
        for team_name, logo_id in config[CONF_TEAM_LOGOS].items():
            logo = await cg.get_variable(logo_id)
            cg.add(var.register_team_logo(team_name, logo))

/// @file
#include "screen_menu_fan_info.hpp"

MI_INFO_PRINT_FAN::MI_INFO_PRINT_FAN()
    : WI_FAN_LABEL_t(_("Print Fan"),
        [](auto) { return FanPWMAndRPM {
                       .pwm = marlin_vars().print_fan_speed,
                       .rpm = marlin_vars().active_hotend().print_fan_rpm,
                   }; } //
    ) {}

MI_INFO_HBR_FAN::MI_INFO_HBR_FAN()
    : WI_FAN_LABEL_t(PRINTER_IS_PRUSA_MK3_5() ? _("Hotend Fan") : _("Heatbreak Fan"),
        [](auto) { return FanPWMAndRPM {
                       .pwm = static_cast<uint8_t>(sensor_data().hbrFan.load()),
                       .rpm = marlin_vars().active_hotend().heatbreak_fan_rpm,
                   }; } //
    ) {}

ScreenMenuFanInfo::ScreenMenuFanInfo()
    : ScreenMenuFanInfo_ {
        // translation: Header text of the screen showing information about fans.
        _("FAN INFO"),
    } {}

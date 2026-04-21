/// @file
#include "screen_menu_sensor_info.hpp"

#include <common/sensor_data.hpp>
#include <screen_move_z.hpp>

#if HAS_DWARF() || HAS_INDX()

MI_INFO_HEAD_PCB_TEMPERATURE::MI_INFO_HEAD_PCB_TEMPERATURE()
    : MenuItemAutoUpdatingLabel {
        _("Head PCB Temperature"),
        standard_print_format::temp_c,
        [](auto) { return sensor_data().HeadBoardTemperature.load(); },
    } {}

MI_INFO_HEAD_MCU_TEMPERATURE::MI_INFO_HEAD_MCU_TEMPERATURE()
    : MenuItemAutoUpdatingLabel {
        _("Head MCU Temperature"),
        standard_print_format::temp_c,
        [](auto) { return sensor_data().HeadMCUTemperature.load(); },
    } {}

#endif

ScreenMenuSensorInfo::ScreenMenuSensorInfo()
    : ScreenMenuSensorInfo_(_("SENSOR INFO")) //
{
    EnableLongHoldScreenAction();
    ClrMenuTimeoutClose();
}

void ScreenMenuSensorInfo::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::HELD_RELEASED) {
        open_move_z_screen();
        return;
    }

    ScreenMenu::windowEvent(sender, event, param);
}

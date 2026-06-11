/// @file
#include "screen_menu_sensor_info.hpp"

#include <common/sensor_data.hpp>
#include <option/has_indx.h>
#include <screen_move_z.hpp>

#if HAS_DWARF() || HAS_INDX()

MI_INFO_HEAD_PCB_TEMPERATURE::MI_INFO_HEAD_PCB_TEMPERATURE()
    : MenuItemAutoUpdatingLabel {
        _("Head PCB Temperature"),
        standard_print_format::temp_c,
        [](auto) { return SensorData::head_pcb_temperature(); },
    } {}

MI_INFO_HEAD_MCU_TEMPERATURE::MI_INFO_HEAD_MCU_TEMPERATURE()
    : MenuItemAutoUpdatingLabel {
        _("Head MCU Temperature"),
        standard_print_format::temp_c,
        [](auto) { return SensorData::head_mcu_temperature(); },
    } {}

#endif

#if HAS_INDX()

MI_INFO_HEAD_AMBIENT_TEMPERATURE::MI_INFO_HEAD_AMBIENT_TEMPERATURE()
    : MenuItemAutoUpdatingLabel {
        _("Head Ambient Temperature"),
        standard_print_format::temp_c,
        [](auto) { return SensorData::head_ambient_temperature(); },
    } {}

MI_INFO_NOZZLE_TEMP_UNCOMPENSATED::MI_INFO_NOZZLE_TEMP_UNCOMPENSATED()
    : MenuItemAutoUpdatingLabel {
        _("Nozzle Raw Temperature"),
        standard_print_format::temp_c,
        [](auto) { return SensorData::nozzle_temp_uncompensated(); },
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

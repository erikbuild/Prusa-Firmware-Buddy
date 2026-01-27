/// @file
#include "dwarf_hotend.hpp"

#include <module/prusa/toolchanger.h>

void DwarfHotend::set_nozzle_target_temp(TargetTemperature set) {
    BaseHotend::set_nozzle_target_temp(set);

    // !!! Do NOT use the set variable, the parent function can crop it
    prusa_toolchanger.getTool(tool_).set_hotend_target_temp(nozzle_target_temp());
}

void DwarfHotend::manage() {
    nozzle_temp_ = prusa_toolchanger.getTool(tool_).get_hotend_temp();

    // Temporary till the temp_hotend.celsius is removed
    thermalManager.temp_hotend[tool_].celsius = nozzle_temp_;

    // !!! MUST be called after temps are set properly
    BaseHotend::manage();
}

/// @file
#include "dwarf_hotend.hpp"

#include <module/prusa/toolchanger.h>
#include <common/aggregate_arity.hpp>

void DwarfHotend::set_nozzle_target_temp(TargetTemperature set) {
    BaseHotend::set_nozzle_target_temp(set);

    // !!! Do NOT use the set variable, the parent function can crop it
    prusa_toolchanger.getTool(tool_).set_hotend_target_temp(nozzle_target_temp());
}

#if HAS_TEMP_HEATBREAK_CONTROL
void DwarfHotend::set_heatbreak_target_temp(TargetTemperature set) {
    BaseHotend::set_heatbreak_target_temp(set);

    prusa_toolchanger.getTool(tool_).set_heatbreak_target_temp(heatbreak_target_temp());
}
#endif

void DwarfHotend::set_nozzle_pid_config(const HotendPIDConfig &set) {
    BaseHotend::set_nozzle_pid_config(set);
    buddy::puppies::dwarfs[tool_].set_pid(set.Kp, set.Ki, set.Kd);
    static_assert(aggregate_arity<HotendPIDConfig>() == 3);
}

void DwarfHotend::manage() {
    auto &tool = prusa_toolchanger.getTool(tool_);

    nozzle_temp_ = tool.get_hotend_temp();
    nozzle_heater_pwm_ = static_cast<uint8_t>(tool.get_heater_pwm());

    heatbreak_temp_ = tool.get_heatbreak_temp();
    heatbreak_fan_pwm_.value = tool.get_heatbreak_fan_pwr();

    // !!! MUST be called after temps are set properly
    BaseHotend::manage();
}

/// @file
#include "indx_hotend.hpp"

#include <puppies/INDX.hpp>
#include <common/aggregate_arity.hpp>
#include <logging/log.hpp>

LOG_COMPONENT_REF(Marlin);

void IndxHotend::set_nozzle_target_temp(TargetTemperature set) {
    if (!stdext::holds_value(PhysicalToolIndex::currently_selected(), tool_)) {
        log_warning(Marlin, "Attempt to set hotend target temp for non-active tool");
        return;
    }
    set_nozzle_target_temp_unchecked(set);
}

void IndxHotend::set_nozzle_target_temp_unchecked(TargetTemperature set) {
    BaseHotend::set_nozzle_target_temp(set);

    // !!! Do NOT use the set variable, the parent function can crop it
    buddy::puppies::indx.set_hotend_target_temp(nozzle_target_temp());
}

void IndxHotend::manage() {
    if (!stdext::holds_value(PhysicalToolIndex::currently_selected(), tool_)) {
        nozzle_temp_ = 15; // INDX_TODO: Fix mintemp so that here can be temperature_invalid
        nozzle_heater_pwm_ = 0;
    } else {
        nozzle_temp_ = buddy::puppies::indx.get_hotend_temp_compensated();
    }

    // !!! MUST be called after temps are set properly
    // BaseHotend::manage();
    manage_temp_residency();
}

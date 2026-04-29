/// @file
#include "indx_hotend.hpp"

#include <puppies/INDX.hpp>
#include <common/aggregate_arity.hpp>
#include <logging/log.hpp>
#include <feature/indx_hotend_temp_model/hotend_temp_model.hpp>

LOG_COMPONENT_REF(Marlin);

void IndxHotend::set_nozzle_target_temp(TargetTemperature set) {
    if (!stdext::holds_value(PhysicalToolIndex::currently_selected(), tool_)) {
        log_warning(Marlin, "Attempt to set hotend target temp for non-active tool");
        return;
    }
    set_nozzle_target_temp_unchecked(set);
}

void IndxHotend::set_nozzle_target_temp_unchecked(TargetTemperature set) {
    const auto old_target = nozzle_target_temp();
    BaseHotend::set_nozzle_target_temp(set);

    // !!! Do NOT use the set variable, the parent function can crop it
    const auto new_target = nozzle_target_temp();

    // Set even if old_target == new_target, just to be sure
    buddy::puppies::indx.set_hotend_target_temp(new_target);

    if (new_target == old_target) {
        return;
    }

    // Changing target temp indicates filament or tool change
    // In both cases, we want the compensator to fetch the new filament parameters
    buddy::hotend_temp_model().reset_state();
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

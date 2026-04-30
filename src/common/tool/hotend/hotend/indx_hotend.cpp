/// @file
#include "indx_hotend.hpp"

#include <puppies/INDX.hpp>
#include <common/aggregate_arity.hpp>
#include <feature/indx_hotend_temp_model/hotend_temp_model.hpp>

void IndxHotend::handle_nozzle_target_change() {
    BaseHotend::handle_nozzle_target_change();
    buddy::puppies::indx.set_hotend_target_temp(nozzle_target_temp());
    buddy::hotend_temp_model().reset_state();
}

void IndxHotend::start_heating() {
    assert_thermally_managed_invariant(NoTool {});
    is_thermally_managed_ = true;
    handle_nozzle_target_change();
}

void IndxHotend::stop_heating() {
    // target doesn't change so the next heating can heat to original target
    buddy::puppies::indx.set_hotend_target_temp(0);
    // tool is still physically on head, but it is no longer thermally managed
    is_thermally_managed_ = false;
    assert_thermally_managed_invariant(NoTool {});
}

void IndxHotend::assert_thermally_managed_invariant(std::variant<PhysicalToolIndex, NoTool> expected_managed) {
    for (auto t : PhysicalToolIndex::all()) {
        if (stdext::holds_value(expected_managed, t)) {
            continue;
        }
        if (IndxHotend::indx_tool(t).hotend().is_thermally_managed()) {
            bsod_unreachable();
        }
    }
}

void IndxHotend::manage() {
    if (is_thermally_managed_) {
        nozzle_temp_ = buddy::puppies::indx.get_hotend_temp_compensated();
    } else {
        nozzle_temp_ = 15; // INDX_TODO: Fix mintemp so that here can be temperature_invalid
        nozzle_heater_pwm_ = 0;
    }
    BaseHotend::manage();
}

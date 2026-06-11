#include "calibration_preamble.hpp"

#include <option/has_indx.h>

#if HAS_INDX()

    #include <Marlin/src/gcode/gcode.h>
    #include <Marlin/src/module/motion.h>
    #include <Marlin/src/module/prusa/toolchanger.h>

    #include <variant>

namespace mapi {

bool calibration_preamble(CalibrationPreambleToolPolicy tool_policy,
    const stdext::inplace_function<void(CalibrationPreambleStep)> &on_step) {

    on_step(CalibrationPreambleStep::moving_away);
    // Lower Z all the way down (stops at endstop) — always first, before any homing or
    // tool picking, so XY moves can't drag the nozzle across the bed
    do_homing_move(AxisEnum::Z_AXIS, Z_MAX_POS, HOMING_FEEDRATE_INVERTED_Z);

    if (tool_policy == CalibrationPreambleToolPolicy::ensure_picked && std::holds_alternative<NoTool>(PhysicalToolIndex::currently_selected())) {
        on_step(CalibrationPreambleStep::picking_tool);
        // Z is already safe at the bottom: skip the Z lift and don't return Z anywhere
        if (!prusa_toolchanger.pick_any_tool(tool_return_t::no_return, {}, tool_change_lift_t::no_lift, false)) {
            return false;
        }
        // pick_any_tool homes XY precisely
    } else {
        on_step(CalibrationPreambleStep::homing);
        // z_raise=0: Z is already safely at the bottom from the homing move above
        GcodeSuite::G28_no_parser(true, true, false, { .z_raise = 0, .precise = false });
    }
    return true;
}

} // namespace mapi

#endif // HAS_INDX()

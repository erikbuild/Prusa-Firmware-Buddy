#pragma once

#include <inplace_function.hpp>
#include <option/has_indx.h>

#if HAS_INDX()

namespace mapi {

/// Motion step about to be performed; lets each wizard report its own FSM phase.
enum class CalibrationPreambleStep {
    moving_away, ///< lowering the bed to the physical bottom
    picking_tool, ///< picking a tool (ensure_picked policy only, fired only when no tool is currently picked)
    homing, ///< homing XY
};

enum class CalibrationPreambleToolPolicy {
    keep_as_is, ///< only home XY (e.g. dock calibration works with an empty head)
    ensure_picked, ///< pick any tool when none is picked
};

/// Makes subsequent XY moves safe regardless of a possibly unhomed/stale Z:
/// lowers the bed to the physical bottom (endstop-protected homing move),
/// then homes XY (picking a tool homes XY too, so no separate homing is needed).
/// The on_step callback fires before each motion step so wizards can report their FSM phase.
/// @return false when the tool pick fails (callers treat it as abort)
bool calibration_preamble(CalibrationPreambleToolPolicy tool_policy,
    const stdext::inplace_function<void(CalibrationPreambleStep)> &on_step);

} // namespace mapi

#endif // HAS_INDX()

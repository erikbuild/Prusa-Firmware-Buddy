/**
 * @file
 */
#include "src/module/prusa/toolchanger.h"
#include "../gcode.h"
#include "PrusaGcodeSuite.hpp"
#include <module/tool_change.h>
#include <mapi/motion.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### P0: Park extruder (tool)
 *
 * Internal GCode
 *
 * Only XL & printers with INDX tools
 *
 *#### Usage
 *
 *    P0 [ S | L | D ]
 *
 *#### Parameters
 *
 * - `S` - Don't move the tool in XY after change
 *
 * - `L` - Z Lift settings
 *   - `0` - no lift
 *   - `1` - lift by max MBL diff
 *   - `2` - full lift(default)
 * - `D` - Z lift return settings
 *   - `0` - Do not return in Z after lift
 *   - `1` - Normal return
 *
 * - `R[mm]` - Distance to retract during the parking moves
 * - `V[mm/s]` - Retraction feedrate (independent on the move feedrate)
 */
void PrusaGcodeSuite::P0() {

    static_assert(EXTRUDERS > 1);

    GcodeSuite::get_destination_from_command(); // sets destination = current position or user request

    // by default, Tx goes to specified destination or current position, unless following:
    tool_return_t return_type = tool_return_t::no_return;

    auto z_lift = static_cast<tool_change_lift_t>(parser.byteval('L', static_cast<uint8_t>(tool_change_lift_t::full_lift)));
    if (z_lift > tool_change_lift_t::_last_item) {
        z_lift = tool_change_lift_t::full_lift; // invalid input, use full_lift
    }
    bool z_down = parser.byteval('D', 1);

    // TODO: Make retraction in parallel with the parking (BFW-8942)
    if (const float retract_mm = parser.floatval('R', 0); retract_mm != 0 && PhysicalToolIndex::currently_selected_opt().has_value()) {
        mapi::extruder_move(-retract_mm, parser.floatval('V', 40));
    }

    tool_change(NoTool {}, return_type, z_lift, z_down);
}
/** @}*/

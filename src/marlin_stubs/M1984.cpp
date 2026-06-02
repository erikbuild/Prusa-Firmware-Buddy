#include <option/has_indx.h>

#if HAS_INDX()

    #include "PrusaGcodeSuite.hpp"
    #include <module/prusa/toolchanger.h>
    #include <tool_index.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1984: Manually park a stuck nozzle into a dock
 *
 *#### Usage
 *
 *    M1984 [T<dock>]
 *
 *#### Parameters
 *
 * - `T` - Dock index (0-based). When omitted, the nozzle-mismatch FSM asks
 *   the user to pick a dock interactively.
 */
void PrusaGcodeSuite::M1984() {
    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    std::optional<PhysicalToolIndex> tool;
    if (const auto t = p.option<uint8_t>('T', static_cast<uint8_t>(0), static_cast<uint8_t>(PhysicalToolIndex::count - 1))) {
        tool = PhysicalToolIndex::from_raw(*t);
    }

    prusa_toolchanger.manual_tool_park(tool);
}

/** @}*/

#endif

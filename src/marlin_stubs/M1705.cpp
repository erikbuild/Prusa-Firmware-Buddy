#include "PrusaGcodeSuite.hpp"

#include <logging/log.hpp>
#include <feature/auto_retract/auto_retract.hpp>
#include <module/planner.h>

LOG_COMPONENT_REF(PRUSA_GCODE);

namespace PrusaGcodeSuite {

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1705: Autoretract
 *
 *#### Usage
 *
 *    M1705
 *
 */
void M1705() {
    if (std::holds_alternative<NoTool>(PhysicalToolIndex::currently_selected())) {
        log_error(PRUSA_GCODE, "autoretract on invalid tool");
        assert(false);
        return;
    }

    buddy::auto_retract().maybe_retract_from_nozzle();
    planner.synchronize();
}

/** @}*/

} // namespace PrusaGcodeSuite

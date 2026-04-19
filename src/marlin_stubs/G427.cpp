/// @file
/// @brief G427: Full tool offset calibration (Z-offset via probing + XY-offset with tool_offset board)

#include "PrusaGcodeSuite.hpp"
#include <feature/tool_offset_calibration/tool_offset_calibration.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### G427: Tool offset calibration
 *
 * Runs the full tool offset calibration sequence for all mapped tools:
 *  1. Determine which physical tools are needed from tool mapping + spool join
 *  2. For each tool calibrate XYZ
 *  3. Set results to runtime variables and save to EEPROM
 *
 *#### Usage
 *
 *    G427 [R | P]
 *
 *#### Parameters
 *
 * - `R` - millimeters of random jitter on X & Y axis during z_probing each tool <0;255>
 * - `P` - number of Z probe repetitions per point to average (default 1) <1;255>
 */
namespace PrusaGcodeSuite {

void G427() {
    GCodeParser2 parser;
    if (!parser.parse_marlin_command()) {
        return;
    }

    uint8_t r_param = 0;
    (void)parser.store_option_if_present('R', r_param);

    uint8_t p_param = 1;
    (void)parser.store_option_if_present('P', p_param);
    p_param = std::max<uint8_t>(p_param, 1);

    tool_offset_calibration::run(r_param, p_param);
}

} // namespace PrusaGcodeSuite

/** @}*/

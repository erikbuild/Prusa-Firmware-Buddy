/// @file
/// @brief M1985: INDX tool offsets calibration wizard

#include <option/has_indx.h>

#if HAS_INDX()

    #include "PrusaGcodeSuite.hpp"
    #include <feature/indx_tool_offsets_calibration/indx_tool_offsets_calibration.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1985: INDX tool offsets calibration
 *
 * Runs the selftest wizard that calibrates the XY offset of every physical tool against the
 * tool offset sensor and updates the stored sensor position. Z offsets are left untouched (they
 * are populated by G427 at print start).
 *
 *#### Usage
 *
 *    M1985
 */
void PrusaGcodeSuite::M1985() {
    indx_tool_offsets_calibration::run();
}

/** @}*/

#endif

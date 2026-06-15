#include <option/has_wastebin_fill_tracking.h>
#include <marlin_stubs/PrusaGcodeSuite.hpp>
#include <feature/wastebin_watcher/wastebin_watcher.hpp>

static_assert(HAS_WASTEBIN_FILL_TRACKING());

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1986: Empty the INDX nozzle-cleaner wastebin
 *
 * Internal gcode behind the "Empty Wastebin" menu action: parks the head clear of the cleaner,
 * prompts the user to empty the bin and resets the fill counter on confirm. Lives in WastebinWatcher.
 *
 *#### Usage
 *
 *    M1986
 */
void PrusaGcodeSuite::M1986() {
    WastebinWatcher::instance().pause_to_empty(/*full=*/false);
}

/** @}*/

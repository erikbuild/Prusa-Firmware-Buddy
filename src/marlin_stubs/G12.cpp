#include "PrusaGcodeSuite.hpp"

#include <nozzle_cleaner.hpp>
#include <logging/log.hpp>
#include "common/gcode/inject_queue_actions.hpp"
#include "marlin_server.hpp"

#include <option/has_auto_retract.h>
#if HAS_AUTO_RETRACT()
    #include <feature/auto_retract/auto_retract.hpp>
#endif

#include <option/has_indx.h>

LOG_COMPONENT_REF(PRUSA_GCODE);

/** \addtogroup G-Codes
 * @{
 */

/**
 * ### G12: Clean nozzle on Nozzle Cleaner <a href="https://reprap.org/wiki/G-code#G12:_Clean_Tool">G12: Clean Tool</a>
 *
 * #### Usage
 *
 *     G12 [ R ] [ S ]
 *
 * #### Parameters
 *
 * - `R` - Ensure filament is (auto-)retracted before cleaning
 * - `S` - (INDX only) Cleaning sequence number (default: 0 = clean)
 *         0 = clean, 1 = quick_clean, 2 = deep_clean, 10 = purge_clean,
 *         20 = eject_blob, 90 = enter_cleaner, 91 = exit_cleaner
 *
 */

void PrusaGcodeSuite::G12() {
    GCodeParser2 parser;
    if (!parser.parse_marlin_command()) {
        return;
    }

#if HAS_AUTO_RETRACT()
    bool auto_retract = false;
    (void)parser.store_option_if_present('R', auto_retract);

    if (auto_retract && !buddy::auto_retract().is_safely_retracted_for_unload()) {
        buddy::auto_retract().maybe_retract_from_nozzle();
    }
#endif
#if HAS_INDX()
    {
        uint16_t s_param = std::to_underlying(nozzle_cleaner::Sequence::clean);
        (void)parser.store_option_if_present('S', s_param);

        const auto seq = static_cast<nozzle_cleaner::Sequence>(s_param);
        if (!nozzle_cleaner::is_valid_sequence(seq)) {
            log_warning(PRUSA_GCODE, "G12 S%u: unknown sequence", s_param);
            return;
        }

        const auto &gcode = nozzle_cleaner::get_sequence(seq);
        marlin_server::inject({ GCodeFilename(gcode.filename, gcode.sequence) });
        return;
    }
#endif
    {
        const auto &gcode = nozzle_cleaner::get_sequence(nozzle_cleaner::Sequence::clean);
        marlin_server::inject({ GCodeFilename(gcode.filename, gcode.sequence) });
    }
}

/** @}*/

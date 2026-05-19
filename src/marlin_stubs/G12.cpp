#include "PrusaGcodeSuite.hpp"

#include <nozzle_cleaner.hpp>
#include <logging/log.hpp>

#include <algorithm>
#include <utility>

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
 *         0 = clean, 1 = quick_clean, 2 = deep_clean, 20 = purge_clean,
 *         21 = power_panic_purge, 30 = eject_blob, 90 = enter_cleaner, 91 = exit_cleaner
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

    if (auto_retract && !buddy::auto_retract().is_auto_retracted()) {
        buddy::auto_retract().maybe_retract_from_nozzle();
    }
#endif
    {
        static constexpr std::pair<uint16_t, nozzle_cleaner::Sequence> s_param_map[] = {
            { 0, nozzle_cleaner::Sequence::clean },
#if HAS_INDX()
            { 1, nozzle_cleaner::Sequence::quick_clean },
            { 2, nozzle_cleaner::Sequence::deep_clean },
#endif
            // Reserved for more cleaning sequences
            { 20, nozzle_cleaner::Sequence::purge_clean },
#if HAS_INDX()
            { 21, nozzle_cleaner::Sequence::power_panic_purge },
            // Reserved for more purge sequences
            { 30, nozzle_cleaner::Sequence::eject_blob },
            // Reserved for other sequences
            { 90, nozzle_cleaner::Sequence::enter_cleaner },
            { 91, nozzle_cleaner::Sequence::exit_cleaner },
#endif
        };
        static_assert(std::size(s_param_map) == static_cast<size_t>(nozzle_cleaner::Sequence::_cnt));

        uint16_t s_param = 0;
        (void)parser.store_option_if_present('S', s_param);

        const auto it = std::ranges::find(s_param_map, s_param, &std::pair<uint16_t, nozzle_cleaner::Sequence>::first);
        if (it == std::end(s_param_map)) {
            log_warning(PRUSA_GCODE, "G12 S%u: unknown sequence", s_param);
            return;
        }

        (void)nozzle_cleaner::load_and_execute(it->second);
    }
}

/** @}*/

#include "standard_ramming_sequence.hpp"

#include <bsod/bsod.h>

using namespace buddy;

const RammingSequence &buddy::standard_ramming_sequence(StandardRammingSequence seq, VirtualToolIndex virtual_tool) {
    // TODO different sequence for HF nozzle
    [[maybe_unused]] const bool is_high_flow_nozzle = config_store().get_nozzle_is_high_flow(virtual_tool.to_physical());

    switch (seq) {

#if HAS_AUTO_RETRACT()
    case StandardRammingSequence::auto_retract: {
        static constexpr RammingSequenceArray seq({
            { 8, 995 },
            { -43, 6000 },
            { -8, 3000 },
            { -4, 1800 },
            { 20, 600 },
            { -20, 470 },
            { 55, 1740 },
            { -55, 6000 },
            { 20, 340 },
            { -20, 210 },
        });
        return seq;
    }
#endif

    case StandardRammingSequence::runout: {
        // Shorter - we might have last strand of filament left and standard ramming sequence might get it out of the gears.
        // Also, we don't need a nice tip for re-insertion. The remaining filament is very short and will be thrown out.
        static constexpr RammingSequenceArray seq({
            { 7, 1500 },
            { -50, 2700 },
            { -5, 50 },
            { -50, 1500 },
        });
        return seq;
    }

    case StandardRammingSequence::unload: {
        // Same as auto-retract sequence, just the last push is longer so that the filament gets out of gears
        static constexpr RammingSequenceArray seq({
            { 8, 995 },
            { -43, 6000 },
            { -8, 3000 },
            { -4, 1800 },
            { 20, 600 },
            { -20, 470 },
            { 55, 1740 },
            { -55, 6000 },
            { 20, 340 },
            { -20, 210 },
            { -50, 2000 },
        });
        return seq;
    }
    }

    bsod_unreachable();
}

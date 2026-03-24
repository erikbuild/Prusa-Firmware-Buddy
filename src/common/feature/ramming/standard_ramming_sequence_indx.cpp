#include "standard_ramming_sequence.hpp"

#include <bsod/bsod.h>
#include <config_store/store_instance.hpp>

using namespace buddy;

const RammingSequence &buddy::standard_ramming_sequence(StandardRammingSequence seq, VirtualToolIndex virtual_tool) {
    const bool is_flexible = config_store().get_filament_type(virtual_tool).parameters().is_flexible;

    switch (seq) {

#if HAS_AUTO_RETRACT()
    case StandardRammingSequence::auto_retract: {
        // auto_retract is never called for flexible filaments
        // (filtered in auto_retract.cpp), so no differentiation needed here
        static constexpr RammingSequenceArray seq({
            { 8, 600 },
            { -8, 4800 },
            { 1, 1440 },
            { -1, 1440 },
        });
        return seq;
    }
#endif

    case StandardRammingSequence::runout: {
        if (is_flexible) {
            static constexpr RammingSequenceArray seq({
                { -50, 600 },
            });
            return seq;
        }
        static constexpr RammingSequenceArray seq({
            { 2, 1800 },
            { 12, 1500 },
            { 4, 900 },
            { -24, 4800 },
            { 2, 120 },
            { -2, 120 },
            { 23, 4800 },
            { 1, 1200 },
            { -18, 4800 },
            { -4, 120 },
            { 2, 120 },
            { -30, 3000 },
        });
        return seq;
    }

    case StandardRammingSequence::unload: {
        if (is_flexible) {
            static constexpr RammingSequenceArray seq({
                { -50, 600 },
            });
            return seq;
        }
        static constexpr RammingSequenceArray seq({
            { 2, 1800 },
            { 12, 1500 },
            { 4, 900 },
            { -24, 4800 },
            { 2, 120 },
            { -2, 120 },
            { 23, 4800 },
            { 1, 1200 },
            { -18, 4800 },
            { -4, 120 },
            { 2, 120 },
            { -30, 3000 },
        });
        return seq;
    }
    }

    bsod_unreachable();
}

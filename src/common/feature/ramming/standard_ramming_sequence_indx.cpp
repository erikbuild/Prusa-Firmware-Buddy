#include "standard_ramming_sequence.hpp"

#include <bsod/bsod.h>
#include <config_store/store_instance.hpp>

#include <option/has_auto_retract.h>
#if HAS_AUTO_RETRACT()
    #include <feature/auto_retract/auto_retract.hpp>
#endif

using namespace buddy;

const RammingSequence &buddy::standard_ramming_sequence(StandardRammingSequence seq, VirtualToolIndex virtual_tool) {
    const bool is_flexible = config_store().get_filament_type(virtual_tool).parameters().is_flexible;

    switch (seq) {

#if HAS_AUTO_RETRACT()
    case StandardRammingSequence::auto_retract: {
        // auto_retract is never called for flexible filaments
        // (filtered in auto_retract.cpp), so no differentiation needed here
        static constexpr RammingSequenceArray seq({
            { 8, 480 },
            { -8, 4800 },
            { 1, 1440 },
            { -1, 1440 },
        });
        static_assert(seq.retracted_distance() >= AutoRetract::full_retract_distance);
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
            { 8, 1500 }, // first ramming
            { 8, 1200 },
            { -20, 2100 },
            { -3, 60 }, // cooling moves
            { 3, 120 },
            { 19, 1800 }, // second ramming
            { 2, 600 },
            { -20, 2100 },
            { -3, 120 }, // cooling moves
            { 3, 120 },
            { -6, 120 }, // tip cooling before retraction
            { -8, 360 },
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
            { 8, 1500 }, // first ramming
            { 8, 1200 },
            { -20, 2100 },
            { -3, 60 }, // cooling moves
            { 3, 120 },
            { 19, 1800 }, // second ramming
            { 2, 600 },
            { -20, 2100 },
            { -3, 120 }, // cooling moves
            { 3, 120 },
            { -6, 120 }, // tip cooling before retraction
            { -8, 360 },
        });
        return seq;
    }
    }

    bsod_unreachable();
}

#include "standard_ramming_sequence.hpp"

#include <bsod/bsod.h>

using namespace buddy;

const RammingSequence &buddy::standard_ramming_sequence(StandardRammingSequence seq, [[maybe_unused]] uint8_t hotend) {
    switch (seq) {

#if HAS_AUTO_RETRACT()
    case StandardRammingSequence::auto_retract: {
        static constexpr RammingSequenceArray seq({
            { 0, 100 }, // TODO
        });
        return seq;
    }
#endif

    case StandardRammingSequence::runout: {
        static constexpr RammingSequenceArray seq({
            { 0, 100 }, // TODO
        });
        return seq;
    }

    case StandardRammingSequence::unload: {
        static constexpr RammingSequenceArray seq({
            { 0, 100 }, // TODO
        });
        return seq;
    }
    }

    bsod_unreachable();
}

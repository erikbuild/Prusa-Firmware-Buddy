#include "standard_ramming_sequence.hpp"

#include <bsod/bsod.h>

using namespace buddy;

const RammingSequence &buddy::standard_ramming_sequence(StandardRammingSequence seq, [[maybe_unused]] VirtualToolIndex virtual_tool) {
    switch (seq) {

    case StandardRammingSequence::runout:
    case StandardRammingSequence::unload: {
        static constexpr RammingSequenceArray seq({
            { 20, 1500 },
            { -50, 2700 },
            { -5, 50 },
            { -50, 1500 },
        });
        return seq;
    }
    }

    bsod_unreachable();
}

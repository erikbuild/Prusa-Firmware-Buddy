#pragma once

#include <option/has_auto_retract.h>

#include "ramming_sequence.hpp"
#include <common/tool_index.hpp>

namespace buddy {

enum class StandardRammingSequence {
#if HAS_AUTO_RETRACT()
    auto_retract,
#endif
    runout,
    unload,
};

const RammingSequence &standard_ramming_sequence(StandardRammingSequence seq, VirtualToolIndex virtual_tool);

} // namespace buddy

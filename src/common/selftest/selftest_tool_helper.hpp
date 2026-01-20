#pragma once
#include <stdint.h>
#include "selftest_types.hpp"
#include <tool_index.hpp>

bool is_tool_selftest_enabled(PhysicalToolIndex tool, ToolMask mask);

[[deprecated("Use the ToolIndex overload")]]
inline bool is_tool_selftest_enabled(uint8_t tool, ToolMask mask) {
    return is_tool_selftest_enabled(PhysicalToolIndex::from_raw(tool), mask);
}

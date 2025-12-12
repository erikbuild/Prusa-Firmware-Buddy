#pragma once
#include <stdint.h>
#include "selftest_types.hpp"
#include <tool_index.hpp>

[[deprecated("Use the ToolIndex overload")]]
bool is_tool_selftest_enabled(const uint8_t tool, const ToolMask mask);

inline bool is_tool_selftest_enabled(PhysicalToolIndex tool, const ToolMask mask) {
    return is_tool_selftest_enabled(tool.to_raw(), mask);
}

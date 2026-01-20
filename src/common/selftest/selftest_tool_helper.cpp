#include "selftest_tool_helper.hpp"
#include <option/has_toolchanger.h>
#include "selftest_types.hpp"
#include <utils/variant_utils.hpp>

bool is_tool_selftest_enabled(PhysicalToolIndex tool, ToolMask mask) {
    if (!tool.is_enabled()) {
        return false;
    }

    return match(
        mask,
        [](AllTools) { return true; },
        [&](PhysicalToolIndex t) { return tool == t; } //
    );
}

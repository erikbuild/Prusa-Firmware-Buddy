/// @file
/// @brief Default dock positions for INDX toolchanger.
///
/// INDX-only: the INDX docking mechanism is tolerant of slightly inaccurate
/// positions, so hardcoded defaults are good enough as a starting point before
/// calibration. XL docks require much tighter precision and cannot use simple
/// defaults like these.
#pragma once

#include <option/has_indx.h>
#if !HAS_INDX()
    #error "indx_dock_position_defaults.hpp is INDX-only, do not include for other printers"
#endif

#include "dock_position.hpp"

#include <array>
#include <tool_index.hpp>

namespace indx_dock_position_defaults {

static constexpr auto count = PhysicalToolIndex::count;

static_assert(count == 8, "Update indx_dock_position_defaults if tool count changes");

static constexpr std::array<float, count> x_mm = { 2.0f, 37.0f, 72.0f, 107.0f, 154.0f, 189.0f, 224.0f, 259.0f };
static constexpr float y_mm = -29.f;

static constexpr std::array<DockPosition, count> positions = [] {
    std::array<DockPosition, count> result {};
    for (size_t i = 0; i < count; i++) {
        result[i] = { x_mm[i], y_mm };
    }
    return result;
}();

} // namespace indx_dock_position_defaults

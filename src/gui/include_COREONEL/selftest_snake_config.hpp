#pragma once
#include "i18n.h"
#include <utility_extensions.hpp>
#include <printer_selftest.hpp>
#include <option/has_precise_homing_corexy.h>
#include "selftest_types.hpp"

namespace SelftestSnake {

// Order matters, snake and will be run in the same order, as well as menu items (with indices) will be
enum class Action {
    Fans,
    DoorSensor,
    YCheck,
    XCheck,
#if HAS_PRECISE_HOMING_COREXY()
    PreciseHoming,
#endif
    ZAlign, // also known as z_calib
    Loadcell, // Check loadcell before Z test, because it is used there
    ZCheck,
    Heaters,
    Gears,
    FilamentSensorCalibration,
    PhaseSteppingCalibration,
    _count,
    _last = _count - 1,
    _first = Fans,
};

constexpr bool has_submenu(Action action) {
    switch (action) {
    default:
        return false;
    }
}

constexpr bool requires_toolchanger([[maybe_unused]] Action action) {
    return false;
}

constexpr auto get_submenu_label_template([[maybe_unused]] Action action) -> const char * {
    bsod_unreachable();
}

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
} // namespace SelftestSnake

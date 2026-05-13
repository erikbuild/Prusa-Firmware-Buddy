#pragma once
#include "i18n.h"
#include <utility_extensions.hpp>
#include <printer_selftest.hpp>
#include "selftest_types.hpp"
#include <option/has_precise_homing_corexy.h>

namespace SelftestSnake {

// Order matters, snake and will be run in the same order, as well as menu items (with indices) will be
enum class Action {
    Fans,
    YCheck,
    XCheck,
#if HAS_PRECISE_HOMING_COREXY()
    PreciseHoming,
#endif
    ZAlign, // also known as z_calib
    Loadcell, // Check loadcell before Z test, because it is used there
    ZCheck,
    Heaters,
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

constexpr bool is_multitool_only_action([[maybe_unused]] Action action) {
    return false;
}

constexpr bool requires_toolchanger([[maybe_unused]] Action action) {
    return false;
}

constexpr bool is_singletool_only_action([[maybe_unused]] Action action) {
    return false;
}

consteval auto get_submenu_label_template([[maybe_unused]] Action action) -> const char * {
    consteval_assert_false("This config has no submenu actions");
    return "";
}

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
} // namespace SelftestSnake

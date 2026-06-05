#pragma once
#include "i18n.h"
#include <utility_extensions.hpp>
#include <printer_selftest.hpp>
#include <selftest_types.hpp>

namespace SelftestSnake {

// Order matters, snake and will be run in the same order, as well as menu items (with indices) will be
enum class Action {
    Fans,
    XYCheck,
    ZAlign, // also known as z_calib
    ZCheck,
    Heaters,
    FilamentSensorCalibration,
    FirstLayer,
    _count,
    _last = _count - 1,
    _first = Fans,
};

constexpr auto get_submenu_label_template([[maybe_unused]] Action action) -> const char * {
    bsod_unreachable();
}

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
} // namespace SelftestSnake

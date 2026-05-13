#pragma once
#include "i18n.h"
#include <utility_extensions.hpp>
#include <printer_selftest.hpp>
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
    DockCalibration,
    Loadcell,
    ZCheck,
    Heaters,
    NozzleHeaters,
    Gears,
    FilamentSensorCalibration,
    ToolOffsetsCalibration,
    BedHeaters,
    PhaseSteppingCalibration,
    _count,
    _last = _count - 1,
    _first = Fans,
};

constexpr bool has_submenu(Action action) {
    switch (action) {
    case Action::DockCalibration:
    case Action::Loadcell:
    case Action::FilamentSensorCalibration:
    case Action::Gears:
        return true;
    default:
        return false;
    }
}

constexpr bool is_multitool_only_action(Action action) {
    return action == Action::DockCalibration || action == Action::ToolOffsetsCalibration || action == Action::NozzleHeaters || action == Action::BedHeaters;
}

constexpr bool requires_toolchanger(Action action) {
    return action == Action::DockCalibration || action == Action::ToolOffsetsCalibration;
}

constexpr bool is_singletool_only_action(Action action) {
    return action == Action::Heaters;
}

// Returns a printf-style format string with a single %d for the 1-based tool/dock index.
consteval auto get_submenu_label_template(Action action) -> const char * {
    switch (action) {
    case Action::DockCalibration:
        return N_("Dock %d Calibration");
    case Action::Loadcell:
        return N_("Tool %d Loadcell Test");
    case Action::FilamentSensorCalibration:
        return N_("Tool %d Filament Sensor Calibration");
    case Action::Gears:
        return N_("Tool %d Gearbox alignment");
    default:
        consteval_assert_false("Unable to find a label template for this action");
        return "";
    }
}

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
} // namespace SelftestSnake

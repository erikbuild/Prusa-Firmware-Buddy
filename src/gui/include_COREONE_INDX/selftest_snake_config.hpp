#pragma once
#include "i18n.h"
#include <utility_extensions.hpp>
#include <printer_selftest.hpp>
#include <option/has_precise_homing_corexy.h>
#include "selftest_types.hpp"

namespace SelftestSnake {

// Order matters, snake and will be run in the same order, as well as menu items (with indices) will be
enum class Action {
    DoorSensor,
    XCheck,
    YCheck,
#if HAS_PRECISE_HOMING_COREXY()
    PreciseHoming,
#endif
    DockCalibration,
    NozzleCleanerCalibration,
    FilamentSensorCalibration,
    ZAlign, // also known as z_calib
    Loadcell, // Check loadcell before Z test, because it is used there
    ZCheck,
    Fans,
    Heaters,
    _count,
    _last = _count - 1,
    _first = DoorSensor,
};

template <Action action>
concept SubmenuActionC = false;

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

consteval auto get_submenu_label(PhysicalToolIndex tool, Action action) -> const char * {
    struct ToolText {
        PhysicalToolIndex tool;
        Action action;
        const char *label;
    };
    const ToolText tooltexts[] {
        ToolText {
            .tool = PhysicalToolIndex::from_raw(0),
            .action = Action::_first,
            .label = nullptr }
    };

    if (auto it = std::ranges::find_if(tooltexts, [&](const auto &elem) {
            return elem.tool == tool && elem.action == action;
        });
        it != std::end(tooltexts)) {
        return it->label;
    } else {
        consteval_assert_false("Unable to find a label for this combination");
        return "";
    }
}

struct MenuItemText {
    Action action;
    const char *label;
};

// could have been done with an array of texts directly, but there would be an order dependancy
inline constexpr MenuItemText blank_item_texts[] {
    { Action::DoorSensor, N_("%d Door Sensor") },
        { Action::XCheck, N_("%d X Axis Test") },
        { Action::YCheck, N_("%d Y Axis Test") },
#if HAS_PRECISE_HOMING_COREXY()
        { Action::PreciseHoming, N_("%d Homing Calibration") },
#endif
        { Action::DockCalibration, N_("%d Dock Calibration") },
        { Action::NozzleCleanerCalibration, N_("%d Nozzle Cleaner Calibration") },
        { Action::FilamentSensorCalibration, N_("%d Filament Sensor Calibration") },
        { Action::ZAlign, N_("%d Z Alignment Calibration") },
        { Action::Loadcell, N_("%d Loadcell Test") },
        { Action::ZCheck, N_("%d Z Axis Test") },
        { Action::Fans, N_("%d Fan Test") },
        { Action::Heaters, N_("%d Heater Test") },
};

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
} // namespace SelftestSnake

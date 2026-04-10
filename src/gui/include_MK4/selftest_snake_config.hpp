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
    Loadcell,
    ZCheck,
    Heaters,
    Gears,
    FilamentSensorCalibration,
    _count,
    _last = _count - 1,
    _first = Fans,
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
    { Action::Fans, N_("%d Fan Test") },
    { Action::ZAlign, N_("%d Z Alignment Calibration") },
    { Action::XYCheck, N_("%d XY Axis Test") },
    { Action::Loadcell, N_("%d Loadcell Test") },
    { Action::ZCheck, N_("%d Z Axis Test") },
    { Action::Heaters, N_("%d Heater Test") },
    { Action::Gears, N_("%d Gearbox Alignment") },
    { Action::FilamentSensorCalibration, N_("%d Filament Sensor Calibration") },
};

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
} // namespace SelftestSnake

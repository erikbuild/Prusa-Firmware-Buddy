#pragma once
#include "i18n.h"
#include <utility_extensions.hpp>
#include <printer_selftest.hpp>
#include <option/has_precise_homing_corexy.h>
#include "selftest_types.hpp"
#include <utils/storage/enum_bitset.hpp>

namespace SelftestSnake {

// Order matters, snake and will be run in the same order, as well as menu items (with indices) will be
enum class Action : uint8_t {
    DoorSensor,
    XCheck,
    YCheck,
    BeltTuning,
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
    PhaseSteppingCalibration,
    InputShaper,
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

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
EnumBitset<Action, Action::_count> get_dependencies(Action action);
} // namespace SelftestSnake

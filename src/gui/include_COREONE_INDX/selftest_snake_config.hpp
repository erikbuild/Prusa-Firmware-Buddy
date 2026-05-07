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
    ZAlign, // also known as z_calib
    Loadcell, // Check loadcell before Z test, because it is used there
    ZCheck,
    Fans,
    Heaters,
    FilamentSensorCalibration,
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

consteval auto get_submenu_label_template([[maybe_unused]] Action action) -> const char * {
    consteval_assert_false("This config has no submenu actions");
    return "";
}

constexpr EnumBitset<Action, Action::_count> get_dependencies(Action action) {
    auto deps = EnumBitset<Action, Action::_count> {};

    switch (action) {
    case Action::DoorSensor:
    case Action::Fans:
        break;
    case Action::XCheck:
    case Action::YCheck:
    case Action::ZAlign:
        deps.set(Action::DoorSensor);
        break;
    case Action::BeltTuning:
        deps.set(Action::XCheck);
        deps.set(Action::YCheck);
        break;
#if HAS_PRECISE_HOMING_COREXY()
    case Action::PreciseHoming:
        deps.set(Action::BeltTuning);
        break;
#endif
    case Action::DockCalibration:
        deps.set(Action::PreciseHoming);
        break;
    case Action::NozzleCleanerCalibration:
    case Action::Heaters:
        deps.set(Action::DockCalibration);
        break;
    case Action::Loadcell:
        deps.set(Action::NozzleCleanerCalibration); // if nozzle is hot, it is parked above nozzle cleaner
        break;
    case Action::FilamentSensorCalibration:
        // if filament is loaded, we need to unload (above nozzle cleaner)
        deps.set(Action::Heaters);
        deps.set(Action::NozzleCleanerCalibration);
        break;
    case Action::ZCheck:
        deps.set(Action::Loadcell);
        deps.set(Action::ZAlign);
        break;
    case Action::PhaseSteppingCalibration:
    case Action::InputShaper:
        deps.set(Action::ZCheck);
        break;
    case Action::_count:
        assert(false);
        break;
    }

    return deps;
}

namespace {
    consteval bool check_selftest_ordering() {
        for (auto i = 0; i < static_cast<int>(Action::_count); i++) {
            const auto deps = get_dependencies(static_cast<Action>(i));
            for (auto j = i; j < static_cast<int>(Action::_count); j++) {
                // selftest j goes after i -> if j has dependency on i the ordering is wrong
                if (deps.test(static_cast<Action>(j))) {
                    return false;
                }
            }
        }
        return true;
    }

    static_assert(check_selftest_ordering(), "selftests ordering does not satisfy dependencies");
} // namespace

TestResult get_test_result(Action action, ToolMask tool);
uint64_t get_test_mask(Action action);
} // namespace SelftestSnake

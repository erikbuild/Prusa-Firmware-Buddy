#pragma once

#include <selftest_snake_config.hpp>
#include <printers.h>
#include <option/has_indx.h>
#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

namespace SelftestSnake {

static_assert(Action::_first != Action::_last, "Edge case not handled");

inline bool is_multitool() {
#if HAS_TOOLCHANGER()
    return prusa_toolchanger.is_toolchanger_enabled();
#else
    return false;
#endif
}

Action get_first_action();
Action get_last_action();
Action get_next_action(Action action);

// Range of all actions valid for the current printer configuration, for range-for loops.
// Unlike get_next_action(), the iterator has standard half-open semantics
// incrementing the last action yields end (Action::_count).
class ValidActionsRange {
public:
    class Iterator {
    public:
        explicit Iterator(Action action)
            : action_(action) {}

        Action operator*() const {
            return action_;
        }

        Iterator &operator++();

        bool operator==(const Iterator &) const = default;

    private:
        Action action_;
    };

    Iterator begin() const {
        return Iterator(get_first_action());
    }
    Iterator end() const {
        return Iterator(Action::_count);
    }
};

inline ValidActionsRange valid_actions() {
    return {};
}

constexpr bool is_singletool_only_action([[maybe_unused]] Action action) {
#if PRINTER_IS_PRUSA_XL()
    return action == Action::Heaters;
#else
    return false;
#endif
}

constexpr bool is_multitool_only_action([[maybe_unused]] Action action) {
#if PRINTER_IS_PRUSA_XL()
    return action == Action::DockCalibration
        || action == Action::ToolOffsetsCalibration
        || action == Action::NozzleHeaters
        || action == Action::BedHeaters;
#else
    return false;
#endif
}

constexpr bool has_submenu([[maybe_unused]] Action action) {
#if PRINTER_IS_PRUSA_XL()
    return action == Action::DockCalibration
        || action == Action::Loadcell
        || action == Action::FilamentSensorCalibration
        || action == Action::Gears;
#elif HAS_INDX()
    return action == Action::FilamentSensorCalibration;
#else
    return false;
#endif
}

const char *get_action_label(Action action);

}; // namespace SelftestSnake

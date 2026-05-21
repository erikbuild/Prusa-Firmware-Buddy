/**
 * @file crash_recovery_type.hpp
 * @author Radek Vana
 * @brief crash recovery data to be passed between threads
 * @date 2021-10-29
 */

#pragma once

#include <common/fsm_base_types.hpp>
#include "selftest_sub_state.hpp"

struct Crash_recovery_fsm {
    SelftestSubtestState_t x;
    SelftestSubtestState_t y;

    constexpr fsm::PhaseData Serialize() const {
        return fsm::serialize_data(*this);
    }

    constexpr static Crash_recovery_fsm deserialize(fsm::PhaseData new_data) {
        return fsm::deserialize_data<Crash_recovery_fsm>(new_data);
    }

    bool operator==(const Crash_recovery_fsm &other) const = default;
};

/**
 * @brief Class to transport dwarf states for toolchanger crash between marlin and gui threads.
 */
struct Crash_recovery_tool_fsm {
    uint16_t enabled = 0; ///< Mask of enabled dwarves
    uint16_t parked = 0; ///< Mask of parked dwarves

    bool operator==(const Crash_recovery_tool_fsm &) const = default;

    static constexpr Crash_recovery_tool_fsm deserialize(fsm::PhaseData data) {
        return fsm::deserialize_data<Crash_recovery_tool_fsm>(data);
    }

    constexpr fsm::PhaseData serialize() const {
        return fsm::serialize_data(*this);
    }
};

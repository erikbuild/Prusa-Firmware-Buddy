#pragma once
#include <common/fsm_base_types.hpp>
#include "device/board.h"
#include "option/has_loadcell.h"
#include "option/filament_sensor.h"

#include "config_features.h"
#include "selftest_result.hpp"

constexpr const char *ToString(TestResult res) {
    switch (res) {
    case TestResult::unknown:
        return "Unknown";
    case TestResult::skipped:
        return "Skipped";
    case TestResult::passed:
        return "Passed";
    case TestResult::failed:
        return "Failed";
    default:
        break;
    }
    return "ERROR";
}

constexpr const char *ToString(TestResultNet res) {
    switch (res) {
    case TestResultNet::unknown:
        return "Unknown";
    case TestResultNet::unlinked:
        return "Unlinked";
    case TestResultNet::down:
        return "Down";
    case TestResultNet::no_address:
        return "NoAddress";
    case TestResultNet::up:
        return "Up";
    default:
        break;
    }
    return "ERROR";
}

/**
 * @brief Log all results.
 */
void SelftestResult_Log(const SelftestResult &results);

/**
 * @brief FSM compatible structure to request testing selftest results screen.
 */
class FsmSelftestResult {
    fsm::PhaseData data = {};

public:
    constexpr FsmSelftestResult(uint8_t test_selftest_code) {
        data[0] = 0xff; // Show fake results to test GUI screen
        data[1] = test_selftest_code;
    }

    constexpr FsmSelftestResult() {
        data[0] = 0; // Show results from EEPROM
    }

    constexpr FsmSelftestResult(fsm::PhaseData new_data) {
        Deserialize(new_data);
    }

    constexpr bool is_test_selftest() const {
        return (data[0] != 0);
    }

    constexpr uint8_t test_selftest_code() const {
        return data[1];
    }

    constexpr fsm::PhaseData Serialize() const {
        return data;
    }

    constexpr void Deserialize(fsm::PhaseData new_data) {
        data = new_data;
    }
};

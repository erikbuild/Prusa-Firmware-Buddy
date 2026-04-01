#pragma once
#include <cstdint>
#include <concepts>

/**
 * @brief Generic selftest results.
 */
enum class TestResult : uint8_t {
    unknown = 0,
    skipped = 1,
    passed = 2,
    failed = 3,
};

/**
 * @brief Selftest results for a network interface.
 */
enum class TestResultNet : uint8_t {
    unknown = 0, // test did not run
    unlinked = 1, // wifi not present, eth cable unplugged
    down = 2, // wifi present, eth cable plugged, not selected in lan settings
    no_address = 3, // wifi present, no address obtained from DHCP
    up = 4, // wifi present, eth cable plugged, selected in lan settings
};

namespace test_result {

constexpr TestResult evaluate_results(std::same_as<TestResult> auto... results) {
    static_assert(sizeof...(results) > 0, "Pass at least one result");

    if (((results == TestResult::passed) && ... && true)) { // all passed
        return TestResult::passed;
    } else if (((results == TestResult::failed) || ... || false)) { // any failed
        return TestResult::failed;
    } else if (((results == TestResult::skipped) || ... || false)) { // any skipped
        return TestResult::skipped;
    } else { // only unknowns and passed (max n-1) are left
        return TestResult::unknown;
    }
}

} // namespace test_result

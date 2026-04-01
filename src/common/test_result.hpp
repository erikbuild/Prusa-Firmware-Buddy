#pragma once
#include <cstdint>

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

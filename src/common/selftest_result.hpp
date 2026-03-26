#pragma once

#include <config_store/constants.hpp>
#include <tool_index.hpp>
#include <utils/storage/strong_index_array.hpp>

/**
 * @brief Generic selftest results.
 */
typedef enum {
    TestResult_Unknown = 0,
    TestResult_Skipped = 1,
    TestResult_Passed = 2,
    TestResult_Failed = 3,
} TestResult;

/**
 * @brief Selftest results for a network interface.
 */
typedef enum {
    TestResultNet_Unknown = 0, // test did not run
    TestResultNet_Unlinked = 1, // wifi not present, eth cable unplugged
    TestResultNet_Down = 2, // wifi present, eth cable plugged, not selected in lan settings
    TestResultNet_NoAddress = 3, // wifi present, no address obtained from DHCP
    TestResultNet_Up = 4, // wifi present, eth cable plugged, selected in lan settings
} TestResultNet;

#pragma pack(push, 1)

/**
 * @brief Results for selftests of one tool.
 */
struct SelftestTool {
    TestResult printFan : 2;
    TestResult heatBreakFan : 2;
    TestResult fansSwitched : 2; // encapsuling with HAS_SWITCHED_FAN_TEST macro would introduce problems since selftest_result is saved on eeprom as a whole structure
    TestResult nozzle : 2;
    // BFW-8374: fsensor and sideFsensor are no longer used. Selftest results
    // are derived directly from existence of calibration data, not from
    // separate flags. Do not reuse, or we get confusion on upgrades/downgrades.
    TestResult deprecated_fsensor : 2;
    TestResult loadcell : 2;
    // Same as above.
    TestResult deprecated_sideFsensor : 2;
    TestResult dockoffset : 2;
    TestResult tooloffset : 2;
    TestResult gears : 2;

    bool has_heatbreak_fan_passed();
    TestResult evaluate_fans();

    bool operator==(const SelftestTool &rhs) const = default;
};
static_assert(sizeof(SelftestTool) == 3);

/**
 * @brief Test results compacted in eeprom. Added gearbox alignment result to eeprom for snake selftest compatibility
 */
struct SelftestResult {
    static constexpr size_t tools_count = 6; ///< Number of tool slots in stored layout. Do not change without config store migration.

    SelftestResult() = default;
    TestResult xaxis : 2 {};
    TestResult yaxis : 2 {};
    TestResult zaxis : 2 {};
    TestResult bed : 2 {};
    TestResultNet eth : 3 {};
    TestResultNet wifi : 3 {};
    TestResult zalign : 2 {};
    // This member is no longer used and is kept to allow backwards compatibility with config store
    // It was replaced by a result supporting more than one toolhead, that can
    // be found in SelftTool class
    TestResult deprecated_gears : 2 {};
    StrongIndexArray<SelftestTool, tools_count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> tools {};

    bool operator==(const SelftestResult &) const = default;
};

#pragma pack(pop)

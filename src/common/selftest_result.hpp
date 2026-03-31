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
    TestResult _printFan : 2;
    TestResult _heatBreakFan : 2;
    TestResult _fansSwitched : 2; // encapsuling with HAS_SWITCHED_FAN_TEST macro would introduce problems since selftest_result is saved on eeprom as a whole structure
    TestResult _nozzle : 2;
    // BFW-8374: fsensor and sideFsensor are no longer used. Selftest results
    // are derived directly from existence of calibration data, not from
    // separate flags. Do not reuse, or we get confusion on upgrades/downgrades.
    TestResult _deprecated_fsensor : 2;
    TestResult _loadcell : 2;
    // Same as above.
    TestResult _deprecated_sideFsensor : 2;
    TestResult _dockoffset : 2;
    TestResult _tooloffset : 2;
    TestResult _gears : 2;

    bool operator==(const SelftestTool &rhs) const = default;
};
static_assert(sizeof(SelftestTool) == 3);

/**
 * @brief Test results compacted in eeprom. Added gearbox alignment result to eeprom for snake selftest compatibility
 */
struct SelftestResult {
    static constexpr size_t tools_count = 6; ///< Number of tool slots in stored layout. Do not change without config store migration.

    SelftestResult() = default;
    TestResult _xaxis : 2 {};
    TestResult _yaxis : 2 {};
    TestResult _zaxis : 2 {};
    TestResult _bed : 2 {};
    TestResultNet _eth : 3 {};
    TestResultNet _wifi : 3 {};
    TestResult _zalign : 2 {};
    // This member is no longer used and is kept to allow backwards compatibility with config store
    // It was replaced by a result supporting more than one toolhead, that can
    // be found in SelftTool class
    TestResult _deprecated_gears : 2 {};
    StrongIndexArray<SelftestTool, tools_count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes> _tools {};

    bool operator==(const SelftestResult &) const = default;

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_print_fan(uint8_t tool) const;
    inline TestResult get_print_fan(PhysicalToolIndex tool) const {
        return get_print_fan(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_print_fan(uint8_t tool, TestResult result);
    inline void set_print_fan(PhysicalToolIndex tool, TestResult result) {
        set_print_fan(tool.to_raw(), result);
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_heatbreak_fan(uint8_t tool) const;
    inline TestResult get_heatbreak_fan(PhysicalToolIndex tool) const {
        return get_heatbreak_fan(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_heatbreak_fan(uint8_t tool, TestResult result);
    inline void set_heatbreak_fan(PhysicalToolIndex tool, TestResult result) {
        set_heatbreak_fan(tool.to_raw(), result);
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_fans_switched(uint8_t tool) const;
    inline TestResult get_fans_switched(PhysicalToolIndex tool) const {
        return get_fans_switched(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_fans_switched(uint8_t tool, TestResult result);
    inline void set_fans_switched(PhysicalToolIndex tool, TestResult result) {
        set_fans_switched(tool.to_raw(), result);
    }

    [[deprecated("Use the ToolIndex overload")]]
    bool has_heatbreak_fan_passed(uint8_t tool) const;
    inline bool has_heatbreak_fan_passed(PhysicalToolIndex tool) const {
        return has_heatbreak_fan_passed(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult evaluate_fans(uint8_t tool) const;
    inline TestResult evaluate_fans(PhysicalToolIndex tool) const {
        return evaluate_fans(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_nozzle_heater(uint8_t tool) const;
    inline TestResult get_nozzle_heater(PhysicalToolIndex tool) const {
        return get_nozzle_heater(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_nozzle_heater(uint8_t tool, TestResult result);
    inline void set_nozzle_heater(PhysicalToolIndex tool, TestResult result) {
        set_nozzle_heater(tool.to_raw(), result);
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_loadcell(uint8_t tool) const;
    inline TestResult get_loadcell(PhysicalToolIndex tool) const {
        return get_loadcell(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_loadcell(uint8_t tool, TestResult result);
    inline void set_loadcell(PhysicalToolIndex tool, TestResult result) {
        set_loadcell(tool.to_raw(), result);
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_dock_offset(uint8_t tool) const;
    inline TestResult get_dock_offset(PhysicalToolIndex tool) const {
        return get_dock_offset(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_dock_offset(uint8_t tool, TestResult result);
    inline void set_dock_offset(PhysicalToolIndex tool, TestResult result) {
        set_dock_offset(tool.to_raw(), result);
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_tool_offset(uint8_t tool) const;
    inline TestResult get_tool_offset(PhysicalToolIndex tool) const {
        return get_tool_offset(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_tool_offset(uint8_t tool, TestResult result);
    inline void set_tool_offset(PhysicalToolIndex tool, TestResult result) {
        set_tool_offset(tool.to_raw(), result);
    }

    [[deprecated("Use the ToolIndex overload")]]
    TestResult get_gearbox(uint8_t tool) const;
    inline TestResult get_gearbox(PhysicalToolIndex tool) const {
        return get_gearbox(tool.to_raw());
    }

    [[deprecated("Use the ToolIndex overload")]]
    void set_gearbox(uint8_t tool, TestResult result);
    inline void set_gearbox(PhysicalToolIndex tool, TestResult result) {
        set_gearbox(tool.to_raw(), result);
    }

    TestResult get_xaxis() const;
    void set_xaxis(TestResult result);

    TestResult get_yaxis() const;
    void set_yaxis(TestResult result);

    TestResult get_zaxis() const;
    void set_zaxis(TestResult result);

    TestResult get_bed_heater() const;
    void set_bed_heater(TestResult result);

    TestResultNet get_ethernet() const;
    void set_ethernet(TestResultNet result);

    TestResultNet get_wifi() const;
    void set_wifi(TestResultNet result);

    TestResult get_zalign() const;
    void set_zalign(TestResult result);

    TestResult get_deprecated_gears() const;
};

#pragma pack(pop)

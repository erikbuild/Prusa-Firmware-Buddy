#include <marlin_stubs/PrusaGcodeSuite.hpp>

#include <selftest/fsensor/selftest_fsensors.hpp>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

#include <option/has_indx.h>
#if HAS_INDX()
    #include <feature/filament_sensor/filament_sensors_handler.hpp>
    #include <feature/filament_sensor/filament_sensor_states.hpp>
#endif

namespace PrusaGcodeSuite {

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1981: Filament sensor calibration
 *
 * Internal GCode
 *
 *#### Parameters
 *
 * - `T` - Tool to calibrate
 * - `F` - Bitset of tools to calibrate
 *
 */
void M1981() {
    GCodeParser2 parser;
    if (!parser.parse_marlin_command()) {
        return;
    }

    uint8_t tools = 0;
    std::ignore = parser.store_option_if_present('F', tools);

    if (auto tool = parser.option<uint8_t>('T')) {
        tools |= (1 << *tool);
    }

#if HAS_INDX()
    // 4-tool INDX has no secondary filament sensor board — skip tools 5-8.
    // Partial / other errors fall through and fail loudly per-tool.
    static_assert(PhysicalToolIndex::count == 8);
    bool is_4tool = true;
    for (uint8_t i = 4; i < PhysicalToolIndex::count; ++i) {
        auto *fs = GetSideFSensorIgnoreEnabled(i);
        if (!fs || fs->get_state() != FilamentSensorState::NotConnected) {
            is_4tool = false;
            break;
        }
    }
    if (is_4tool) {
        tools &= 0x0F;
    }

    for (auto tool : PhysicalToolIndex::all()) { // On INDX, check all sensors regardless enabled/disabled state
#else
    for (auto tool : PhysicalToolIndex::all().skip_all_disabled()) {
#endif

        if (!(tools & (1 << tool.to_raw()))) {
            continue;
        }

        using Result = SelftestFSensorsResult;
        switch (run_selftest_fsensors({ .tool = tool.to_raw() })) {
        case Result::success:
        case Result::skipped:
            break;

        case Result::failed:
#if HAS_INDX()
            // On INDX, keep going so the user sees every broken tool in one run.
            break;
#else
            return;
#endif

        case Result::aborted:
            // Things are clear here - the test has been aborted, so stop right there
            return;
        }
    }
}

/** @}*/
} // namespace PrusaGcodeSuite

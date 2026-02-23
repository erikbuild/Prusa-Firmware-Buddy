#include <selftest_result_evaluation.hpp>
#include <selftest_result_type.hpp>
#include <option/has_switched_fan_test.h>
#include <option/has_gearbox_alignment.h>
#include <option/has_selftest.h>
#include <option/has_phase_stepping_selftest.h>
#include <option/filament_sensor.h>
#include <option/has_loadcell.h>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

#include <option/has_sheet_profiles.h>
#if HAS_SHEET_PROFILES()
    #include <common/SteelSheets.hpp>
#endif /* HAS_SHEET_PROFILES() */

#include <config_store/store_instance.hpp>

bool is_selftest_successfully_completed() {
#if DEVELOPER_MODE() || !HAS_SELFTEST() || PRINTER_IS_PRUSA_iX()
    return true;
#endif

    const SelftestResult sr = config_store().selftest_result.get();

    const auto all_passed = [](std::same_as<TestResult> auto... results) -> bool {
        static_assert(sizeof...(results) > 0, "Pass at least one result");

        return ((results == TestResult_Passed) && ...); // all passed
    };

    if (!all_passed(sr.xaxis, sr.yaxis, sr.zaxis, sr.bed)) {
        return false;
    }

#if HAS_SHEET_PROFILES()
    if (!SteelSheets::IsSheetCalibrated(config_store().active_sheet.get())) {
        return false;
    }
#endif /* HAS_SHEET_PROFILES() */

    for (auto tool : PhysicalToolIndex::all()) {
#if HAS_TOOLCHANGER()
        if (!prusa_toolchanger.is_tool_enabled(tool)) {
            continue;
        }

        // Without toolchanger, we don't have tool offsets
        if (prusa_toolchanger.is_toolchanger_enabled()) {
            if (!all_passed(sr.tools[tool].dockoffset, sr.tools[tool].tooloffset)) {
                return false;
            }
        }

#endif

#if HAS_SWITCHED_FAN_TEST()
        if (sr.tools[tool].fansSwitched != TestResult_Passed) {
            return false;
        }
#endif /* HAS_SWITCHED_FAN_TEST() */

#if HAS_GEARBOX_ALIGNMENT()
        if (sr.tools[tool].gears == TestResult_Failed) {
            return false;
        }
#endif /* HAS_GEARBOX_ALIGNMENT */

        // Skipped means the sensor is disabled by the user, which is acceptable.
        const auto fs_result = SelftestSnake::get_fsensor_calibration_result(tool.to_raw());
        if (fs_result != TestResult_Passed && fs_result != TestResult_Skipped) {
            return false;
        }

#if HAS_LOADCELL()
        if (!all_passed(sr.tools[tool].loadcell)) {
            return false;
        }
#endif /* HAS_LOADCELL() */

        if (!all_passed(sr.tools[tool].printFan, sr.tools[tool].heatBreakFan, sr.tools[tool].nozzle)) {
            return false;
        }
    }

#if HAS_PHASE_STEPPING_SELFTEST()
    if (!all_passed(config_store().selftest_result_phase_stepping.get())) {
        return false;
    }
#endif /* HAS_PHASE_STEPPING_SELFTEST() */

    return true;
}

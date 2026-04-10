#include "selftest_snake_config.hpp"
#include <selftest_types.hpp>
#include <selftest_result_evaluation.hpp>
#include <config_store/store_instance.hpp>
#include <tool_index.hpp>

#include <option/has_side_fsensor.h>
#include <option/has_chamber_api.h>
#if HAS_CHAMBER_API()
    #include <feature/chamber/chamber.hpp>
#endif
#include <option/xl_enclosure_support.h>
#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

#include <option/has_side_fsensor_remap.h>
#if HAS_SIDE_FSENSOR_REMAP()
    #include <feature/filament_sensor/filament_sensors_handler_remap.hpp>
#endif

#include <option/has_precise_homing_corexy.h>

#if HAS_PRECISE_HOMING_COREXY()
    #include <module/prusa/homing_corexy.hpp>
#endif

namespace SelftestSnake {
TestResult get_test_result(Action action, ToolMask tool) {
    SelftestResult sr = config_store().selftest_result.get();

    switch (action) {
    case Action::Fans: {
        TestResult res = merge_hotends_evaluations(
            [&](PhysicalToolIndex e) {
                return sr.evaluate_fans(e);
            });
#if HAS_CHAMBER_API()
        switch (buddy::chamber().backend()) {
    #if XL_ENCLOSURE_SUPPORT()
        case buddy::Chamber::Backend::xl_enclosure:
            res = test_result::evaluate_results(res, config_store().xl_enclosure_fan_selftest_result.get());
            break;
    #endif /* XL_ENCLOSURE_SUPPORT() */
        case buddy::Chamber::Backend::none:
            break;
        }
#endif /* HAS_CHAMBER_API() */
        return res;
    }
    case Action::ZAlign:
        return sr.get_zalign();
    case Action::YCheck:
        return sr.get_yaxis();
    case Action::XCheck:
        return sr.get_xaxis();
#if HAS_PRECISE_HOMING_COREXY()
    case Action::PreciseHoming:
        return corexy_home_is_calibrated() ? TestResult::passed : TestResult::unknown;
#endif
    case Action::DockCalibration:
        return merge_hotends(tool, [&](const PhysicalToolIndex e) {
            return sr.get_dock_offset(e);
        });
    case Action::Loadcell:
        return merge_hotends(tool, [&](const PhysicalToolIndex e) {
            return sr.get_loadcell(e);
        });
    case Action::ToolOffsetsCalibration:
        return merge_hotends_evaluations([&](PhysicalToolIndex e) {
            return sr.get_tool_offset(e);
        });
    case Action::ZCheck:
        return sr.get_zaxis();
    case Action::BedHeaters:
        return sr.get_bed_heater();
    case Action::NozzleHeaters:
        return merge_hotends_evaluations([&](PhysicalToolIndex e) {
            return sr.get_nozzle_heater(e);
        });
    case Action::Heaters:
        return test_result::evaluate_results(sr.get_bed_heater(), merge_hotends_evaluations([&](PhysicalToolIndex e) {
            return sr.get_nozzle_heater(e);
        }));
    case Action::FilamentSensorCalibration:
        return merge_hotends(tool, [&](const PhysicalToolIndex e) {
            return get_fsensor_calibration_result(e);
        });
    case Action::PhaseSteppingCalibration:
        return test_result::evaluate_results(config_store().selftest_result_phase_stepping.get());
    case Action::Gears:
        return merge_hotends(tool, [&](const PhysicalToolIndex e) {
            return sr.get_gearbox(e);
        });
    case Action::_count:
        break;
    }
    return TestResult::unknown;
}

uint64_t get_test_mask(Action action) {
    switch (action) {
    case Action::YCheck:
        return stmYAxis;
    case Action::XCheck:
        return stmXAxis;
    case Action::ZCheck:
        return stmZAxis;
    case Action::Heaters:
        return stmHeaters;
    case Action::BedHeaters:
        return stmHeaters_bed;
    case Action::NozzleHeaters:
        return stmHeaters_noz;
    case Action::Loadcell:
        return stmLoadcell;
    case Action::ZAlign:
        return stmZcalib;
    case Action::DockCalibration:
        return stmDocks;
    case Action::ToolOffsetsCalibration:
        return stmToolOffsets;

    case Action::Gears:
#if HAS_PRECISE_HOMING_COREXY()
    case Action::PreciseHoming:
#endif
    case Action::FilamentSensorCalibration:
    case Action::Fans:
    case Action::PhaseSteppingCalibration:
    case Action::_count:
        // Implemented as a gcode/invalid
        bsod_unreachable();
    }

    bsod_unreachable();
}

} // namespace SelftestSnake

#include "selftest_fsensors.hpp"
#include "selftest_fsensors_config.hpp"

#include <selftest/selftest_invocation.hpp>
#include <marlin_server_types/fsm/selftest_fsensors_phases.hpp>
#include <marlin_server.hpp>
#include <raii/scope_guard.hpp>
#include <Marlin/src/module/tool_change.h>
#include <M70X.hpp>
#include <feature/filament_sensor/calibrator/filament_sensor_calibrator.hpp>
#include <inplace_vector.hpp>
#include <mapi/motion.hpp>
#include <raii/auto_restore.hpp>
#include <option/has_side_fsensor.h>
#include <option/has_adc_side_fsensor.h>
#include <option/has_nextruder.h>
#include <mapi/cold_extrude.hpp>
#include <option/has_indx.h>
#include <option/has_side_fsensor_invertible.h>

#include <option/has_mmu2.h>
#if HAS_MMU2()
    #include <feature/prusa/MMU2/mmu2_mk4.h>
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

LOG_COMPONENT_REF(FSensor);

namespace {

using Phase = PhaseSelftestFSensors;

#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()

    #if HAS_NEXTRUDER()
/// Used when waiting for the user to insert the filament into the extruder and confirm
constexpr feedRate_t extruder_assist_slow_feedrate = 3;

/// Used when unloading the filament or doing the extra load
constexpr feedRate_t extruder_assist_fast_feedrate = 20;

/// Safe distance we can assist with the insertion of the filament by
/// Basically distance between the filament gear engagement and nozzle
constexpr float assisted_insertion_safe_distance_mm = 70;

/// After the user confirms that he has inserted the filament (in assist mode), load this extra amount
/// to ensure that the filament sensor is fully engaged
/// Extra load should still not exceed assisted_insertion_safe_distance_mm
constexpr float assisted_insertion_extra_load_distance_mm = 10;

/// How much the filament wiggles forward and back during calibration
constexpr float calibration_wiggle_distance_mm = 10;
    #endif

// These values are calibrated for the nextruder. They need to be tweaked for different extruders.
static_assert(HAS_NEXTRUDER());

#endif

class SelftestFSensors {

public:
    using Result = SelftestFSensorsResult;

public:
    SelftestFSensors(const SelftestFSensorsParams &params)
        : params_(params) {
    }

    [[nodiscard]] Result run();

private:
    void initialize();
    void finalize();

    [[nodiscard]] bool prepare();

    enum class InitialRemoveResult {
        continue_,
        aborted,
        skipped,
    };
    [[nodiscard]] InitialRemoveResult initial_remove_filament();

    [[nodiscard]] InitialRemoveResult remove_filament_early_check();
    void calibrate(FilamentSensorCalibrator::CalibrationPhase phase);
    [[nodiscard]] bool ask_insert_filament();
    [[nodiscard]] bool ask_remove_filament();
    [[nodiscard]] Result process_and_present_results(bool aborted = false);

#if PRINTER_IS_PRUSA_MINI()
    [[nodiscard]] bool ask_mini_has_fsensor();
#endif

    enum class AskFilamentCommonResult {
        not_ready,
        ready,
        aborted
    };
    [[nodiscard]] AskFilamentCommonResult ask_filament_common(FilamentSensorCalibrator::CalibrationPhase calib_phase, PhaseSelftestFSensors not_ready_phase, PhaseSelftestFSensors ready_phase);

    enum class EarlyFailCheckResult {
        retry,
        continue_,
        abort,
    };
    [[nodiscard]] EarlyFailCheckResult check_early_fail(FilamentSensorCalibrator::CalibrationPhase calib_phase);

    /// Wraps fsm_change to always include the tool index in PhaseData[0]
    void fsm_change_with_tool(Phase phase) {
        marlin_server::fsm_change(phase, { params_.tool });
    }

private:
    const SelftestFSensorsParams params_;

    static constexpr size_t max_fsensor_count = HAS_SIDE_FSENSOR() ? 2 : 1;

    std::array<FilamentSensorCalibrator::Storage, max_fsensor_count> calibrators_storage_;
    stdext::inplace_vector<FilamentSensorCalibrator *, max_fsensor_count> calibrators_ {};

    ///
    bool ignore_early_fail_ = false;
};

SelftestFSensorsResult SelftestFSensors::run() {
#if PRINTER_IS_PRUSA_MINI()
    if (!ask_mini_has_fsensor()) {
        marlin_server::fsm_destroy(ClientFSM::SelftestFSensors);
        return Result::success;
    }
#endif

    initialize();
    ScopeGuard exit_guard = [&] {
        finalize();
    };

    log_info(FSensor, "Calibration started");

    // Set up for the selftest - pick the right tool, park to the right position, ...
    if (!prepare()) {
        return Result::aborted;
    }

    // Make sure there is no filament in the filament sensor before continuing
    switch (initial_remove_filament()) {
    case InitialRemoveResult::continue_:
        break;
    case InitialRemoveResult::aborted:
        return Result::aborted;
    case InitialRemoveResult::skipped:
        // Leave the previous per-tool result untouched and move on.
        return Result::skipped;
    }

    // Aborting from this point on means test failure, because it means that the user was likely not able to continue further
    // So show the results and store that the selftest failed
    ScopeGuard fail_guard = [&] {
        log_info(FSensor, "Selftest failed due to manual abort");
        (void)process_and_present_results(true);
    };

    // If we detect straight on that there is something wrong with one of the sensors, give the user again an opportunity to do an unload
    // Aborting here fails the selftest; skipping leaves the previous result untouched.
    switch (remove_filament_early_check()) {
    case InitialRemoveResult::continue_:
        break;
    case InitialRemoveResult::aborted:
        return Result::aborted;
    case InitialRemoveResult::skipped:
        fail_guard.disarm();
        return Result::skipped;
    }

    /// Reinserting the filament multiple times could give us better calibration data
    static constexpr auto round_count = 1;
    for (int i = 0; i < round_count; i++) {
        if (i > 0) {
            if (!ask_remove_filament()) {
                return Result::aborted;
            }
        }

        /// Calibrate the nins phase
        calibrate(FilamentSensorCalibrator::CalibrationPhase::not_inserted);

        // No point asking the user to insert filament for a sensor that already failed.
        // Fall through to process_and_present_results so the per-tool failure is recorded
        // and the caller (M1981) can continue to the next tool.
        if (std::ranges::all_of(calibrators_, [](const auto *c) { return c->failed(); })) {
            break;
        }

        if (!ask_insert_filament()) {
            return Result::aborted;
        }

        /// Calibrate the ins phase
        calibrate(FilamentSensorCalibrator::CalibrationPhase::inserted);
    }

#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
    /// We should not leave the filament in the gears. Move back in the last round.
    mapi::ColdExtrudeGuard cold_extrude_guard;
    mapi::extruder_move(-assisted_insertion_safe_distance_mm, extruder_assist_fast_feedrate);
#endif

    fail_guard.disarm();
    return process_and_present_results();
}

#if PRINTER_IS_PRUSA_MINI()
bool SelftestFSensors::ask_mini_has_fsensor() {
    marlin_server::fsm_change(Phase::ask_mini_has_fsensor);
    const auto response = marlin_server::wait_for_response(Phase::ask_mini_has_fsensor);
    switch (response) {

    case Response::Yes:
        return true;

    case Response::No:
        return false;

    default:
        bsod_unreachable();
    }
}
#endif

void SelftestFSensors::initialize() {
    config_store().fsensor_enabled.set(true);

    const auto add_calibrator = [&](IFSensor *sensor) -> bool {
        if (!sensor) {
            return false;
        }

        auto calibrator = sensor->create_calibrator(calibrators_storage_[calibrators_.size()]);
        if (!calibrator) {
            return false;
        }

        calibrators_.push_back(calibrator);
        return true;
    };

    if (add_calibrator(GetExtruderFSensor(params_.tool))) {
        config_store().fsensor_extruder_enabled_bits.transform([&](auto val) { return val | (1 << params_.tool); });
    }

#if HAS_SIDE_FSENSOR()
    #if HAS_MMU2()
    // If MMU is enabled, the side fsensor would end up being the one from the MMU.
    if (!config_store().mmu2_enabled.get())
    #endif
    {
        auto *sensor = GetSideFSensor(params_.tool);
        if (sensor && add_calibrator(sensor)) {
            config_store().fsensor_side_enabled_bits.transform([&](auto val) { return val | (1 << params_.tool); });
        }
    }
#endif

#if HAS_MMU2()
    // This selftest does not work with the MMU. Disable it for the duration (gets reenabled in finalize)
    MMU2::mmu2.Stop();
#endif

    // Make sure that the config store changes are processed by the fsensors manager
    FSensors_instance().request_enable_state_update();
    while (FSensors_instance().is_enable_state_update_processing() && !planner.draining()) {
        idle(true);
    }
}

void SelftestFSensors::finalize() {
    marlin_server::fsm_destroy(ClientFSM::SelftestFSensors);

#if HAS_MMU2()
    // We have disabled the MMU for the test, possibly re-enable it
    if (config_store().mmu2_enabled.get()) {
        filament_gcodes::mmu_on();
    }
#endif
}

bool SelftestFSensors::prepare() {
    fsm_change_with_tool(Phase::prepare);

#if HAS_TOOLCHANGER() && !HAS_INDX()
    if (prusa_toolchanger.is_toolchanger_enabled()) {
        const auto toolchange_result = prusa_toolchanger.tool_change(PhysicalToolIndex::from_raw_notool(params_.tool), tool_return_t::no_return, {}, tool_change_lift_t::full_lift, false);
        if (!toolchange_result) {
            return false;
        }
    }
#endif

    return true;
}

SelftestFSensors::InitialRemoveResult SelftestFSensors::initial_remove_filament() {
    while (true) {
        {
            fsm_change_with_tool(Phase::offer_unload);
            const auto response = marlin_server::wait_for_response(Phase::offer_unload);
            switch (response) {

            case Response::Continue:
                break;

            case Response::Unload: {
                const auto examined_virtual_tool = VirtualToolIndex::from_raw(params_.tool);
#if HAS_TOOLCHANGER() && HAS_INDX()
                const auto examined_physical_tool = examined_virtual_tool.to_physical();
                if (!stdext::holds_value(PhysicalToolIndex::currently_selected(), examined_physical_tool)) {
                    const auto tool_change_result = prusa_toolchanger.tool_change(examined_physical_tool, tool_return_t::no_return, {}, tool_change_lift_t::full_lift, false);
                    if (!tool_change_result) {
                        continue; // Ask user again to unload
                    }
                }
#endif
                filament_gcodes::M702_unload({}, Z_AXIS_LOAD_POS, RetAndCool_t::Neither, examined_virtual_tool, false);
            } break;

            case Response::Skip:
                return InitialRemoveResult::skipped;

            case Response::Abort:
                return InitialRemoveResult::aborted;

            default:
                bsod_unreachable();
            }
        }

        {
            fsm_change_with_tool(Phase::ask_filament);
            const auto response = marlin_server::wait_for_response(Phase::ask_filament);
            switch (response) {

            case Response::Yes:
                // Again offer unload
                continue;

            case Response::No:
#if HAS_SIDE_FSENSOR_INVERTIBLE()
                // No filament confirmed -> let polarity-aware calibrators align their inversion bit, then return
                for (auto *calibrator : calibrators_) {
                    calibrator->calibrate_polarity();
                }
#endif
                return InitialRemoveResult::continue_;

            case Response::Abort:
                return InitialRemoveResult::aborted;

            default:
                bsod_unreachable();
            }
        }
    }
}

SelftestFSensors::InitialRemoveResult SelftestFSensors::remove_filament_early_check() {
    while (true) {
        switch (check_early_fail(FilamentSensorCalibrator::CalibrationPhase::not_inserted)) {

        case EarlyFailCheckResult::abort:
            return InitialRemoveResult::aborted;

        case EarlyFailCheckResult::continue_:
            return InitialRemoveResult::continue_;

        case EarlyFailCheckResult::retry:
            switch (initial_remove_filament()) {
            case InitialRemoveResult::continue_:
                break; // Continue the while loop to re-check.
            case InitialRemoveResult::aborted:
                return InitialRemoveResult::aborted;
            case InitialRemoveResult::skipped:
                return InitialRemoveResult::skipped;
            }
            break;
        }
    }
}

void SelftestFSensors::calibrate(FilamentSensorCalibrator::CalibrationPhase phase) {
    log_info(FSensor, "Calibrating phase %i", (int)phase);

    fsm_change_with_tool(Phase::calibrating);

#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
    mapi::ColdExtrudeGuard cold_extrude_guard;
    float extruded_distance = 0;
#endif

    const auto start_time = ticks_ms();
    while (ticks_diff(ticks_ms(), start_time) < 2000) {
#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
        // Wiggle the extruder forward and back, it affects the fsensor measurements
        // So we get bigger range of measured values
        // !!! We need to start by wiggling forward to make sure we don't disengage the filament
        if (!planner.busy()) {
            const float distance = extruded_distance <= 0 ? calibration_wiggle_distance_mm : -calibration_wiggle_distance_mm;
            mapi::extruder_move(distance, extruder_assist_fast_feedrate);
            extruded_distance += distance;
        }
#endif

        for (auto *calibrator : calibrators_) {
            calibrator->calibrate(phase);
        }

        idle(true);
    }

#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
    mapi::extruder_move(-extruded_distance, extruder_assist_fast_feedrate);
    planner.synchronize();
#endif
}

SelftestFSensors::AskFilamentCommonResult SelftestFSensors::ask_filament_common(FilamentSensorCalibrator::CalibrationPhase calib_phase, PhaseSelftestFSensors not_ready_phase, PhaseSelftestFSensors ready_phase) {
    bool is_ready = true;
    for (auto *calibrator : calibrators_) {
        is_ready &= calibrator->is_ready_for_calibration(calib_phase);
    }
    const auto phase = is_ready ? ready_phase : not_ready_phase;

    fsm_change_with_tool(phase);
    switch (marlin_server::get_response_from_phase(phase)) {

    case Response::Continue:
        switch (check_early_fail(calib_phase)) {

        case EarlyFailCheckResult::continue_:
            return AskFilamentCommonResult::ready;

        case EarlyFailCheckResult::abort:
            return AskFilamentCommonResult::aborted;

        case EarlyFailCheckResult::retry:
            return AskFilamentCommonResult::not_ready;
        }
        bsod_unreachable();

    case Response::Abort:
        return AskFilamentCommonResult::aborted;

    case Response::_none:
        return AskFilamentCommonResult::not_ready;

    default:
        bsod_unreachable();
    }
}

SelftestFSensors::EarlyFailCheckResult SelftestFSensors::check_early_fail([[maybe_unused]] FilamentSensorCalibrator::CalibrationPhase calib_phase) {
#if HAS_SIDE_FSENSOR()
    const uint8_t cnt_failed = std::ranges::count_if(calibrators_, [](const auto *c) { return c->failed(); });
    const uint8_t cnt_not_ready = std::ranges::count_if(calibrators_, [&](const auto *c) { return !c->is_ready_for_calibration(calib_phase); });

    if (cnt_failed > 0 || cnt_not_ready > 0) {
        if (!ignore_early_fail_) {
            fsm_change_with_tool(Phase::not_ready_confirm_continue);
            switch (marlin_server::wait_for_response(Phase::not_ready_confirm_continue)) {

            case Response::Continue:
                ignore_early_fail_ = true;
                break;

            case Response::Retry:
                return EarlyFailCheckResult::retry;

            case Response::Abort:
                return EarlyFailCheckResult::abort;

            default:
                bsod_unreachable();
            }
        }

        // Fail calibrators that are not ready; otherwise a missing calibrate() follow-up
        // could cause the selftest to be reported as OK.
        for (auto *calibrator : calibrators_) {
            calibrator->fail_if(!calibrator->is_ready_for_calibration(calib_phase));
        }
    }
#endif

    return EarlyFailCheckResult::continue_;
}

bool SelftestFSensors::ask_insert_filament() {
#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
    mapi::ColdExtrudeGuard cold_extrude_guard;
    float inserted_distance = 0;

    // On failure, revert to the original extruder position
    ScopeGuard reset_extruder_guard = [&] {
        mapi::extruder_move(-inserted_distance, extruder_assist_fast_feedrate);
        planner.synchronize();
    };
#endif

    for (bool is_ready = false; !is_ready;) {
        // Note: using if-else here because switch wouldn't allow us to break out of the loop easily
        const auto ask_result = ask_filament_common(FilamentSensorCalibrator::CalibrationPhase::inserted, Phase::insert_filament_not_ready, Phase::insert_filament_ready);
        switch (ask_result) {

        case AskFilamentCommonResult::ready:
            // Break out of the loop immediately
            is_ready = true;
            continue;

        case AskFilamentCommonResult::aborted:
            return false;

        case AskFilamentCommonResult::not_ready:
            // Continue the loop
            break;
        }

#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
        inserted_distance += mapi::extruder_schedule_turning(extruder_assist_slow_feedrate);

        // If we've inserted too much, quickly unload to make sure we don't try to force the filament into a cold nozzle
        if (inserted_distance >= assisted_insertion_safe_distance_mm - assisted_insertion_extra_load_distance_mm - calibration_wiggle_distance_mm) {
            mapi::extruder_move(-inserted_distance, extruder_assist_fast_feedrate);
            planner.synchronize();
            inserted_distance = 0;

            // If user clicked on "continue" right before the printer did the quick unload, throw away that response as we have probably lost the filament
            marlin_server::clear_fsm_response(ClientFSM::SelftestFSensors);
        }
#endif

        idle(true);
    }

#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
    reset_extruder_guard.disarm();
    mapi::extruder_move(assisted_insertion_extra_load_distance_mm, extruder_assist_fast_feedrate);
    planner.synchronize();
#endif

    return true;
}

bool SelftestFSensors::ask_remove_filament() {
#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
    mapi::ColdExtrudeGuard cold_extrude_guard;
#endif

    while (true) {
        // Note: using if-else here because switch wouldn't allow us to break out of the loop easily
        const auto ask_result = ask_filament_common(FilamentSensorCalibrator::CalibrationPhase::not_inserted, Phase::remove_filament_not_ready, Phase::remove_filament_ready);
        if (ask_result == AskFilamentCommonResult::ready) {
            break;

        } else if (ask_result == AskFilamentCommonResult::aborted) {
            return false;

        } else {
            assert(ask_result == AskFilamentCommonResult::not_ready);
        }

#if SELFTEST_FSENSOR_EXTRUDER_ASSIST()
        mapi::extruder_schedule_turning(-extruder_assist_fast_feedrate);
#endif
        idle(true);
    }

    return true;
}

SelftestFSensorsResult SelftestFSensors::process_and_present_results(bool aborted) {
    bool success = !aborted;

    for (auto *calibrator : calibrators_) {
        // If we didn't complete all the steps of the calibration, the calibrator will have incomplete data
        // So just fail the calibration
        calibrator->fail_if(aborted);

        // Do the final evaulation and store the calibration results
        calibrator->finish();

        const bool result = !calibrator->failed();
        log_info(FSensor, "Calibration %i %i Result: %i", int(calibrator->sensor().id().position), int(calibrator->sensor().id().index), result);
        success &= result;

#if HAS_SIDE_FSENSOR()
        // Persist the side-sensor verification result so callers that don't store
        // their own calibration data (XBuddy Extension sensors) can report it later.
        if (calibrator->sensor().id().position == FilamentSensorID::Position::side) {
            config_store().selftest_result_side_fsensor.set(calibrator->sensor().id().index, result ? TestResult::passed : TestResult::failed);
        }
#endif
    }

    // Make sure that the calibration results are reflected
    FSensors_instance().request_enable_state_update();
    while (FSensors_instance().is_enable_state_update_processing() && !planner.draining()) {
        idle(true);
    }

    const auto phase = success ? Phase::success : Phase::failed;
    fsm_change_with_tool(phase);

    marlin_server::wait_for_response(phase);

    return success ? Result::success : Result::failed;
}

} // namespace

SelftestFSensorsResult run_selftest_fsensors(const SelftestFSensorsParams &params) {
    SelftestFSensors test { params };
    const auto result = test.run();
    if (result == SelftestFSensorsResult::aborted) {
        selftest_invocation::mark_aborted();
    }
    return result;
}

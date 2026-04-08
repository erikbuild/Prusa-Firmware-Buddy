#include "indx_dock_calibration.hpp"
#include <test_result.hpp>

#include <client_response.hpp>
#include <common/fsm_base_types.hpp>
#include <gcode/gcode.h>
#include <marlin_server.hpp>
#include <module/endstops.h>
#include <module/motion.h>
#include <module/planner.h>
#include <module/stepper.h>
#include <module/stepper/indirection.h>
#include <module/prusa/toolchanger.h>
#include <module/prusa/homing_corexy.hpp>
#include <puppies/INDX.hpp>
#include "dock_calibration_data.hpp"

#include <bitset>
#include <tool_index.hpp>
#include <logging/log.hpp>
#include <config_store/store_instance.hpp>
#include <common/selftest_result.hpp>
#include <raii/scope_guard.hpp>

LOG_COMPONENT_DEF(DockCalibration, logging::Severity::info);

using marlin_server::wait_for_response;

namespace indx_dock_calibration {

/// Feedrate for Z lowering [mm/s]
static constexpr feedRate_t Z_LOWER_FEEDRATE = HOMING_FEEDRATE_INVERTED_Z;

/// How far to move in Y to clear the dock after measurement [mm]
static constexpr float DOCK_EXIT_Y_MM = 30.0f;

/// How far to move in -X to clear the last dock before homing [mm]
static constexpr float LAST_DOCK_EXIT_X_MM = 40.0f;

/// Serialize dock index into PhaseData (4 bytes)
static fsm::PhaseData serialize_dock_data(PhysicalToolIndex tool) {
    fsm::PhaseData data {};
    data[0] = tool.to_raw();
    return data;
}

class DockCalibrationWizard {
public:
    void run() {
        // Disable nozzle check for the duration of calibration (RAII restores on exit)
        const bool nozzle_check_was_disabled = prusa_toolchanger.is_nozzle_check_disabled();
        prusa_toolchanger.set_nozzle_check_disabled(true);
        ScopeGuard nozzle_check_guard([nozzle_check_was_disabled] {
            prusa_toolchanger.set_nozzle_check_disabled(nozzle_check_was_disabled);
        });

        const auto result = run_inner();

        // Store calibration result (abort leaves it unchanged)
        auto sr = config_store().selftest_result.get();
        switch (result) {
        case Result::success:
            sr.set_dock_offset(PhysicalToolIndex::from_raw(0), TestResult::passed);
            break;
        case Result::failed:
            sr.set_dock_offset(PhysicalToolIndex::from_raw(0), TestResult::failed);
            break;
        case Result::aborted:
            break;
        }
        config_store().selftest_result.set(sr);

        // Disable motors at the end of the wizard
        disable_XY();
        disable_Z();

        if (result == Result::success) {
            fsm_change(PhaseDockCalibration::calibration_success);
            wait_for_response(PhaseDockCalibration::calibration_success);
        }
    }

private:
    marlin_server::FSM_Holder holder { PhaseDockCalibration::intro };

    void fsm_change(PhaseDockCalibration phase, fsm::PhaseData data = {}) {
        marlin_server::fsm_change(phase, data);
    }

    enum class Result {
        success,
        failed,
        aborted,
    };

    /// Bitmask of docks selected for calibration
    std::bitset<PhysicalToolIndex::count> selected_docks;
    static_assert(PhysicalToolIndex::count <= 8, "selected_docks is transferred as uint8_t via FSMResponseVariant");

    Result run_inner() {
        // Intro
        fsm_change(PhaseDockCalibration::intro);
        if (wait_for_response(PhaseDockCalibration::intro) == Response::Abort) {
            return Result::aborted;
        }

        // Check if nozzle is present — ask user to remove tool
        if (auto nozzle = buddy::puppies::indx.get_nozzle_present(); nozzle.has_value() && nozzle.value()) {
            fsm_change(PhaseDockCalibration::remove_tool);
            if (wait_for_response(PhaseDockCalibration::remove_tool) == Response::Abort) {
                return Result::aborted;
            }
            bsod_unreachable();
        }

        // Select how many docks the user has
        uint8_t dock_count = PhysicalToolIndex::count;
        fsm_change(PhaseDockCalibration::select_dock_count);
        {
            const auto response = wait_for_response(PhaseDockCalibration::select_dock_count);
            switch (response) {
            case Response::Docks4:
                dock_count = 4;
                break;
            case Response::Docks8:
            case Response::Other:
                dock_count = 8;
                break;
            default:
                bsod_unreachable();
            }
        }

        // Select which docks to calibrate — pass dock_count via PhaseData
        {
            fsm::PhaseData data {};
            data[0] = dock_count;
            fsm_change(PhaseDockCalibration::select_docks, data);
        }
        {
            const auto response = marlin_server::wait_for_response_variant(PhaseDockCalibration::select_docks);
            if (const auto *raw_mask = response.value_maybe<uint8_t>()) {
                selected_docks = std::bitset<PhysicalToolIndex::count>(*raw_mask);
            } else {
                return Result::aborted;
            }

            if (selected_docks.none()) {
                return Result::aborted;
            }
        }

        // Lower Z all the way down (stops at endstop)
        fsm_change(PhaseDockCalibration::moving_away);
        {
            TemporaryGlobalEndstopsState endstops_guard(true);
            do_homing_move(AxisEnum::Z_AXIS, Z_MAX_POS, Z_LOWER_FEEDRATE);
        }

        // Home XY (z_raise=0: Z is already safely at the bottom from the homing move above)
        fsm_change(PhaseDockCalibration::homing);
        GcodeSuite::G28_no_parser(true, true, false, { .z_raise = 0, .precise = false });

        // Ensure head locking mechanism is open (no tool present)
        prusa_toolchanger.ensure_head_open();

        // Calibrate selected docks
        for (auto tool : PhysicalToolIndex::all()) {
            if (!selected_docks.test(tool.to_raw())) {
                continue;
            }
            const auto r = calibrate_dock(tool);
            if (r != Result::success) {
                return r;
            }
        }

        return Result::success;
    }

    Result calibrate_dock(PhysicalToolIndex tool) {
        for (;;) { // Retry loop for the entire dock calibration
            // Move in front of the dock so the user has a shorter distance to adjust
            current_position.x = PrusaToolChanger::DOCK_DEFAULT_X_MM[tool];
            current_position.y = PrusaToolChanger::DOCK_DEFAULT_Y_MM + PrusaToolChanger::DOCK_SAFE_Y_OFFSET;
            line_to_current_position(PrusaToolChanger::TRAVEL_MOVE_MM_S);
            planner.synchronize();

            // Position and confirm loop — user can go back to reposition
            for (;;) {
                // Disable XY motors so user can move the head
                planner.synchronize();
                disable_XY();

                // Ask user to position head at dock
                fsm_change(PhaseDockCalibration::ask_position_dock, serialize_dock_data(tool));
                if (wait_for_response(PhaseDockCalibration::ask_position_dock) == Response::Abort) {
                    return Result::aborted;
                }

                // Re-enable motors to lock the head in place
                enable_XY();

                // Ask user to confirm position before moving
                fsm_change(PhaseDockCalibration::lock_position, serialize_dock_data(tool));
                const auto response = wait_for_response(PhaseDockCalibration::lock_position);
                if (response == Response::Abort) {
                    return Result::aborted;
                }
                if (response == Response::Continue) {
                    break; // Position confirmed, proceed to measurement
                }
                // Response::Back — loop back to repositioning
            }

            // Measuring
            fsm_change(PhaseDockCalibration::measuring, serialize_dock_data(tool));

            // Reset current position to (0,0) as reference point for measurement
            planner.synchronize();
            current_position.x = 0;
            current_position.y = 0;
            sync_plan_position();

            // Record stepper positions before homing
            const ab_steps_t position_before = { { {
                .x = stepper.position_from_startup(AxisEnum::A_AXIS),
                .y = stepper.position_from_startup(AxisEnum::B_AXIS),
            } } };

            // Move Y to clear the dock before homing
            current_position.y += DOCK_EXIT_Y_MM;
            line_to_current_position(PrusaToolChanger::SLOW_MOVE_MM_S);
            planner.synchronize();

            // Dock 8 (index 7): move away in -X to avoid hardware collision with motor during homing
            if (tool.to_raw() == PhysicalToolIndex::count - 1) {
                current_position.x -= LAST_DOCK_EXIT_X_MM;
                line_to_current_position(PrusaToolChanger::SLOW_MOVE_MM_S);
                planner.synchronize();
            }

            // Home XY (z_raise=0: Z is at the bottom, no need for Z clearance)
            GcodeSuite::G28_no_parser(true, true, false, { .z_raise = 0, .precise = false });

            // Record stepper positions after homing
            const ab_steps_t position_after = { { {
                .x = stepper.position_from_startup(AxisEnum::A_AXIS),
                .y = stepper.position_from_startup(AxisEnum::B_AXIS),
            } } };

            // Convert AB stepper difference to XY mm (CoreXY transform)
            // Dock position = home position + (position_before - position_after)
            MachinePosXY diff;
            corexy_ab_to_xy(position_before - position_after, diff);

            const PrusaToolInfo measured = {
                .dock_x = diff.x + current_position.x,
                .dock_y = diff.y + current_position.y + 1.0f, // Add 1mm to compensate for the fact that the tool is not perfectly flush with the dock surface
            };

            // Validate
            if (!prusa_toolchanger.is_tool_info_valid(tool, measured)) {
                log_error(DockCalibration,
                    "Dock %u position %.1f, %.1f out of bounds",
                    tool.to_raw(), static_cast<double>(measured.dock_x), static_cast<double>(measured.dock_y));

                fsm_change(PhaseDockCalibration::calibration_failed,
                    DockCalibrationFailedData { tool, measured.dock_x, measured.dock_y }.serialize());
                if (wait_for_response(PhaseDockCalibration::calibration_failed) == Response::Retry) {
                    continue; // Retry this dock from the beginning
                }

                // Invalidate calibration for this dock since measurement failed
                auto calibrated_mask = config_store().indx_dock_calibrated_mask.get();
                calibrated_mask.reset(tool.to_raw());
                config_store().indx_dock_calibrated_mask.set(calibrated_mask);

                return Result::failed;
            }

            // Apply
            prusa_toolchanger.set_tool_info(tool, measured);

            // Mark dock as calibrated in persistent store
            auto calibrated_mask = config_store().indx_dock_calibrated_mask.get();
            calibrated_mask.set(tool.to_raw());
            config_store().indx_dock_calibrated_mask.set(calibrated_mask);

            // Persist dock positions after each successful calibration
            prusa_toolchanger.save_tool_info();

            log_info(DockCalibration,
                "Dock %u calibrated: x=%.2f y=%.2f",
                tool.to_raw(), static_cast<double>(measured.dock_x), static_cast<double>(measured.dock_y));

            return Result::success;
        }
    }
};

void run() {
    DockCalibrationWizard wizard;
    wizard.run();
}

} // namespace indx_dock_calibration

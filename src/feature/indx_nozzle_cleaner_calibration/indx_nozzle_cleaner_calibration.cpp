#include "indx_nozzle_cleaner_calibration.hpp"
#include <test_result.hpp>

#include <bsod/bsod.h>
#include <client_response.hpp>
#include <common/fsm_base_types.hpp>
#include <gcode/gcode.h>
#include <gcode/temperature/M104_M109.hpp>
#include <marlin_server.hpp>
#include <module/endstops.h>
#include <module/motion.h>
#include <module/planner.h>
#include <module/stepper.h>
#include <module/stepper/indirection.h>
#include <module/prusa/toolchanger.h>
#include <module/prusa/corexy_transform.hpp>
#include <logging/log.hpp>
#include <config_store/store_instance.hpp>
#include <common/selftest_result.hpp>
#include <common/mapi/calibration_preamble.hpp>
#include <common/mapi/parking.hpp>
#include <feature/gcode_exception/gcode_exception.hpp>
#include <tool/hotend/hotend.hpp>
#include <utils/variant_utils.hpp>
#include <selftest/selftest_invocation.hpp>

LOG_COMPONENT_DEF(NozzleCleanerCalibration, logging::Severity::info);

using marlin_server::wait_for_response;

namespace indx_nozzle_cleaner_calibration {

/// Hotend is considered cool enough to touch below this temperature [°C]
static constexpr int16_t cooldown_safe_temperature_c = 50;

/// Max tolerance (+/-) in both axes
static constexpr float offset_tolerance_mm = 3.0f;

/// Exit measurement point (general for both axes)
static constexpr float exit_move_mm = -30.f;

/// Per-axis calibration configuration
struct AxisCalibConfig {
    PhaseNozzleCleanerCalibration phase_ask_position;
    PhaseNozzleCleanerCalibration phase_lock_position;
    PhaseNozzleCleanerCalibration phase_measuring;
    PhaseNozzleCleanerCalibration phase_evaluating;
    AxisEnum axis;
    /// Expected position in machine coordinates [mm]
    float nominal_mm;
    /// Park position before disabling motors (user will move it from there).
    /// Slightly offset from the calibration point to avoid collision if not calibrated.
    xy_pos_t park_pos;
};

static constexpr AxisCalibConfig y_axis_config {
    .phase_ask_position = PhaseNozzleCleanerCalibration::ask_position_y,
    .phase_lock_position = PhaseNozzleCleanerCalibration::lock_position_y,
    .phase_measuring = PhaseNozzleCleanerCalibration::measuring_y,
    .phase_evaluating = PhaseNozzleCleanerCalibration::evaluating_y,
    .axis = AxisEnum::Y_AXIS,
    .nominal_mm = Y_NOZZLE_CLEANER_ORIGIN,
    .park_pos = { .x = X_NOZZLE_CLEANER_ORIGIN - 10.f, .y = Y_NOZZLE_CLEANER_ORIGIN },
};

static constexpr AxisCalibConfig x_axis_config {
    .phase_ask_position = PhaseNozzleCleanerCalibration::ask_position_x,
    .phase_lock_position = PhaseNozzleCleanerCalibration::lock_position_x,
    .phase_measuring = PhaseNozzleCleanerCalibration::measuring_x,
    .phase_evaluating = PhaseNozzleCleanerCalibration::evaluating_x,
    .axis = AxisEnum::X_AXIS,
    .nominal_mm = X_NOZZLE_CLEANER_ORIGIN,
    .park_pos = { .x = X_WASTEBIN_SAFE_POINT, .y = Y_BRUSH_AVOID_POINT },
};

class NozzleCleanerCalibrationWizard {
public:
    void run() {
        const auto result = run_inner();

        // Store calibration result (abort leaves it unchanged)
        switch (result) {
        case Result::success:
            config_store().selftest_result_nozzle_cleaner_calibration.set(TestResult::passed);
            break;
        case Result::failed:
            config_store().selftest_result_nozzle_cleaner_calibration.set(TestResult::failed);
            break;
        case Result::aborted:
            selftest_invocation::mark_aborted();
            break;
        }

        // Disable motors at the end of the wizard
        disable_XY();
        disable_Z();

        if (result == Result::success) {
            fsm_change(PhaseNozzleCleanerCalibration::calibration_success);
            wait_for_response(PhaseNozzleCleanerCalibration::calibration_success);
        }
    }

private:
    marlin_server::FSM_Holder holder { PhaseNozzleCleanerCalibration::intro };

    enum class Result {
        success,
        failed,
        aborted,
    };

    void fsm_change(PhaseNozzleCleanerCalibration phase, fsm::PhaseData data = {}) {
        marlin_server::fsm_change(phase, data);
    }

    Result run_inner() {
        // Intro
        fsm_change(PhaseNozzleCleanerCalibration::intro);
        if (wait_for_response(PhaseNozzleCleanerCalibration::intro) == Response::Abort) {
            return Result::aborted;
        }

        if (!mapi::calibration_preamble(mapi::CalibrationPreambleToolPolicy::ensure_picked, [&](mapi::CalibrationPreambleStep step) {
                switch (step) {
                case mapi::CalibrationPreambleStep::moving_away:
                    fsm_change(PhaseNozzleCleanerCalibration::moving_away);
                    break;
                case mapi::CalibrationPreambleStep::picking_tool:
                    fsm_change(PhaseNozzleCleanerCalibration::picking_tool);
                    break;
                case mapi::CalibrationPreambleStep::homing:
                    fsm_change(PhaseNozzleCleanerCalibration::homing);
                    break;
                case mapi::CalibrationPreambleStep::parking_tool:
                    bsod_unreachable();
                }
            })) {
            return Result::aborted;
        }

        // Wait for the nozzle to cool down before the user handles the head
        if (const auto result = wait_for_nozzle_cooldown(); result != Result::success) {
            return result;
        }

        // One-time mechanical Z screw adjustment
        mapi::park({ .x = x_axis_config.park_pos.x, .y = x_axis_config.park_pos.y });
        disable_XY();
        fsm_change(PhaseNozzleCleanerCalibration::move_to_z_point);
        if (wait_for_response(PhaseNozzleCleanerCalibration::move_to_z_point) == Response::Abort) {
            return Result::aborted;
        }

        // Calibrate X first, then Y.
        {
            const auto result = calibrate_axis(x_axis_config);
            if (result != Result::success) {
                return result;
            }
        }
        {
            const auto result = calibrate_axis(y_axis_config);
            if (result != Result::success) {
                return result;
            }
        }

        return Result::success;
    }

    /// Wait until the currently selected hotend cools below a safe touch temperature.
    /// Pushes the current temperature to the GUI each idle tick.
    Result wait_for_nozzle_cooldown() {
        const auto tool = stdext::get_optional<PhysicalToolIndex>(PhysicalToolIndex::currently_selected());
        if (!tool.has_value()) {
            bsod_unreachable();
        }

        const auto current_temp = Hotend::for_tool(*tool).nozzle_temp();
        if (current_temp <= cooldown_safe_temperature_c) {
            return Result::success;
        }

        fsm_change(PhaseNozzleCleanerCalibration::wait_for_nozzle_cooldown);

        // Interrupt the blocking M109 the instant Abort is pressed. The handler resumes queuing on
        // scope exit. Cooldown does no moves, so there are no skipped steps to recover.
        GCodeExceptionHandler abort_handler { GCEHandlerExtent::extruder_only, [] {} };

        // Push current temp to the GUI and handle abort while M109 is blocking
        Subscriber subscriber(marlin_server::idle_publisher, [tool, &abort_handler] {
            if (marlin_server::get_response_from_phase(PhaseNozzleCleanerCalibration::wait_for_nozzle_cooldown) == Response::Abort) {
                gcode_exceptions().throw_at(&abort_handler);
                return;
            }
            // Read the live temperature each tick so the GUI reflects the actual cooldown progress
            const uint16_t t = static_cast<uint16_t>(Hotend::for_tool(*tool).nozzle_temp());
            const fsm::PhaseData data = {
                static_cast<uint8_t>((t >> 8) & 0xff),
                static_cast<uint8_t>(t & 0xff),
                0,
                0,
            };
            marlin_server::fsm_change(PhaseNozzleCleanerCalibration::wait_for_nozzle_cooldown, data);
        });

        if (current_temp > cooldown_safe_temperature_c) {
            const M109Flags flags {
                .target_temp = cooldown_safe_temperature_c,
                .wait_heat_or_cool = true,
                .autotemp = true,
            };
            M109_no_parser(*tool, flags); // This is the temp we want to reach
            Hotend::for_tool(*tool).set_nozzle_target_temp(0); // This is so that we dont accidentally re-heat to 50
        }

        return gcode_exceptions().is_unwinding() ? Result::aborted : Result::success;
    }

    /// @return success if axis calibrated, failed on measurement failure, aborted on user abort
    Result calibrate_axis(const AxisCalibConfig &config) {
        for (;;) { // Retry loop for this axis

            // Move close to the calibration point so the user has a shorter distance to adjust
            mapi::park({ .x = config.park_pos.x, .y = config.park_pos.y });

            // Position and confirm loop — user can go back to reposition
            for (;;) {
                // Disable motors so user can position the head
                planner.synchronize();
                disable_XY();

                // User positions head for this axis measurement
                fsm_change(config.phase_ask_position);
                if (wait_for_response(config.phase_ask_position) == Response::Abort) {
                    return Result::aborted;
                }

                // Re-enable motors to lock position
                enable_XY();

                // Ask user to confirm position before measuring
                fsm_change(config.phase_lock_position);
                const auto response = wait_for_response(config.phase_lock_position);
                if (response == Response::Abort) {
                    return Result::aborted;
                }
                if (response == Response::Continue) {
                    break; // Position confirmed, proceed to measurement
                }
                // Response::Back — loop back to repositioning
            }

            // Measuring
            fsm_change(config.phase_measuring);

            // Reset current position to 0 on the measured axis as reference point
            planner.synchronize();
            current_position.x = 0;
            current_position.y = 0;
            sync_plan_position();

            // Record stepper positions before homing
            const ab_steps_t position_before = { { {
                .x = stepper.position_from_startup(AxisEnum::A_AXIS),
                .y = stepper.position_from_startup(AxisEnum::B_AXIS),
            } } };

            // On the Y measure point, move out in -X to clear the cleaner before homing. The X measure
            // point is in the V groove; G28 homes Y first (away from the cleaner) and then X, so no exit
            // move is needed there.
            if (config.axis == AxisEnum::Y_AXIS) {
                current_position.x += exit_move_mm;
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
            MachinePosXY diff;
            corexy_ab_to_xy(position_before - position_after, diff);

            // Fold the picked tool's hotend offset into the measurement so the stored cleaner
            // position is tool-independent. The raw `diff` represents the carriage displacement;
            // the nozzle (which is what the user placed in the V-groove) sits at
            // carriage + hotend_offset, so adding the offset gives the V-groove position in
            // machine coordinates that any tool can later be steered to via G750.
            const auto selected_tool = stdext::get_optional<PhysicalToolIndex>(PhysicalToolIndex::currently_selected());
            const xyz_pos_t tool_offset = selected_tool ? hotend_offset[*selected_tool] : xyz_pos_t {};

            // Extract the measured position for this axis
            const float measured = (config.axis == AxisEnum::X_AXIS)
                ? (diff.x + current_position.x + tool_offset.x)
                : (diff.y + current_position.y + tool_offset.y);

            const float offset = measured - config.nominal_mm;

            // Validate offset is within bounds
            if (std::abs(offset) > offset_tolerance_mm) {
                log_error(NozzleCleanerCalibration,
                    "Nozzle cleaner %c offset %.2f out of bounds (max %.1f mm)",
                    (config.axis == AxisEnum::X_AXIS) ? 'X' : 'Y',
                    static_cast<double>(offset), static_cast<double>(offset_tolerance_mm));

                fsm_change(config.phase_evaluating, fsm::serialize_data(EvaluatingData::from(offset, config.nominal_mm)));
                if (wait_for_response(config.phase_evaluating) == Response::Retry) {
                    continue; // Retry this axis
                }
                return Result::failed;
            }

            // Store offset for this axis
            if (config.axis == AxisEnum::X_AXIS) {
                config_store().nozzle_cleaner_x_origin_offset.set(offset);
            } else {
                config_store().nozzle_cleaner_y_origin_offset.set(offset);
            }

            log_info(NozzleCleanerCalibration,
                "Nozzle cleaner %c calibrated: offset=%.2f",
                (config.axis == AxisEnum::X_AXIS) ? 'X' : 'Y',
                static_cast<double>(offset));

            return Result::success;
        }
    }
};

void run() {
    NozzleCleanerCalibrationWizard wizard;
    wizard.run();
}

} // namespace indx_nozzle_cleaner_calibration

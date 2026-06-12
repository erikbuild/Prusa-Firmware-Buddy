#include "indx_tool_offsets_calibration.hpp"

#include <client_response.hpp>
#include <common/fsm_base_types.hpp>
#include <common/mapi/calibration_preamble.hpp>
#include <common/selftest_result.hpp>
#include <config_store/store_instance.hpp>
#include <feature/tool_offset_calibration/tool_offset_calibration.hpp>
#include <logging/log.hpp>
#include <marlin_server.hpp>
#include <selftest/selftest_invocation.hpp>
#include <test_result.hpp>

LOG_COMPONENT_DEF(ToolOffsetsCalibration, logging::Severity::info);

using marlin_server::wait_for_response;

namespace indx_tool_offsets_calibration {

namespace {

    class Wizard {
    public:
        void run() {
            const auto result = run_inner();

            switch (result) {
            case Result::success:
                config_store().selftest_result_tool_offsets_calibration.set(TestResult::passed);
                break;
            case Result::failed:
                config_store().selftest_result_tool_offsets_calibration.set(TestResult::failed);
                break;
            case Result::aborted:
                selftest_invocation::mark_aborted();
                break;
            }

            if (result == Result::success) {
                fsm_change(PhaseToolOffsetsCalibration::calibration_success);
                wait_for_response(PhaseToolOffsetsCalibration::calibration_success);
            } else if (result == Result::failed) {
                fsm_change(PhaseToolOffsetsCalibration::calibration_failed);
                wait_for_response(PhaseToolOffsetsCalibration::calibration_failed);
            }
        }

    private:
        marlin_server::FSM_Holder holder { PhaseToolOffsetsCalibration::intro };

        enum class Result {
            success,
            failed,
            aborted,
        };

        void fsm_change(PhaseToolOffsetsCalibration phase, fsm::PhaseData data = {}) {
            marlin_server::fsm_change(phase, data);
        }

        Result run_inner() {
            // Intro
            fsm_change(PhaseToolOffsetsCalibration::intro);
            if (wait_for_response(PhaseToolOffsetsCalibration::intro) == Response::Abort) {
                return Result::aborted;
            }

            // The nozzle cleaner is not calibrated yet at this stage of the selftest, so we can't
            // auto-purge/clean. Ask the user to ensure nozzles are clean before we heat & probe.
            fsm_change(PhaseToolOffsetsCalibration::ensure_nozzles_clean);
            if (wait_for_response(PhaseToolOffsetsCalibration::ensure_nozzles_clean) == Response::Abort) {
                return Result::aborted;
            }

            // Make the bed provably safe (it may be at an unknown height with Z unhomed) and
            // get a tool picked / XY homed before any moves over the bed.
            if (!mapi::calibration_preamble(mapi::CalibrationPreambleToolPolicy::ensure_picked, [this](mapi::CalibrationPreambleStep step) {
                    switch (step) {
                    case mapi::CalibrationPreambleStep::moving_away:
                        fsm_change(PhaseToolOffsetsCalibration::moving_away);
                        break;
                    case mapi::CalibrationPreambleStep::picking_tool:
                        fsm_change(PhaseToolOffsetsCalibration::picking_tool);
                        break;
                    case mapi::CalibrationPreambleStep::homing:
                        fsm_change(PhaseToolOffsetsCalibration::homing);
                        break;
                    case mapi::CalibrationPreambleStep::parking_tool:
                        bsod_unreachable();
                    }
                })) {
                return Result::aborted;
            }

            // Run the actual tool-offset calibration in Calibration context (no auto-clean, no
            // print abort). Abort responsiveness is at tool boundaries — when the user presses
            // Abort, the next progress callback (start of the next tool's iteration) returns
            // false, unwinding tool_offset_calibration::run() cleanly.
            fsm_change(PhaseToolOffsetsCalibration::calibrating);
            bool aborted_by_user = false;
            const auto progress_cb = [this, &aborted_by_user](const tool_offset_calibration::ProgressReport &p) -> bool {
                fsm_change(PhaseToolOffsetsCalibration::calibrating,
                    fsm::serialize_data(ProgressData::from(p.step, p.total_steps, p.tool.to_raw())));
                if (marlin_server::get_response_from_phase(PhaseToolOffsetsCalibration::calibrating) == Response::Abort) {
                    aborted_by_user = true;
                    return false;
                }
                return true;
            };
            if (!tool_offset_calibration::run(0, 1, tool_offset_calibration::Context::Calibration, progress_cb)) {
                return aborted_by_user ? Result::aborted : Result::failed;
            }

            return Result::success;
        }
    };

} // namespace

void run() {
    Wizard wizard;
    wizard.run();
}

} // namespace indx_tool_offsets_calibration

// Contactless tool offset measurement
//
// Measures XY (and Z) offset of the current nozzle relative to an inductive
// sensor mounted on the bed. The key challenge is that we do not know the
// time delay between commanding a step and seeing the corresponding sensor
// response, so we cannot simply map a peak in the sensor signal to a
// physical position.
//
// The trick is to sweep the nozzle over the sensor at two different speeds.
// Each sweep consists of four passes: forward-slow, backward-slow,
// forward-fast, backward-fast. Because the sensor response is roughly
// symmetric around the nozzle-sensor alignment, we detect the symmetry axis
// of each pass via correlation. This gives us four peak times t1..t4.
//
// From the motion profile we know the exact relationship between peak times
// and two unknowns: the nozzle position (relative to sweep start) and the
// sensor time delay. The two speeds break the degeneracy — a pure position
// shift moves all four peaks equally, while a pure time delay shifts them
// by amounts proportional to the speed. A least-squares fit over the four
// observed peak times recovers both unknowns simultaneously, without
// requiring any clock synchronization between the motion system and the
// sensor.
//
// Each axis (X, Y) is measured independently.
// There is a FSM driving the process described later.
//
// Motion is driven via signal2step (direct step enqueueing) rather than the
// Marlin planner. The planner would buffer and reshape the moves in ways we
// cannot predict — junction deviation, look-ahead merging, etc. — so the
// actual velocity profile would not match the one we use to build the
// theoretical peak-time model. With signal2step we generate the exact
// trapezoidal velocity waveform we want, convert it to step events
// ourselves, and feed them straight into precise_stepping. This gives us a
// known, deterministic motion profile that the fitting step can rely on.

#include "contactless_offset.hpp"

#include <gcode/gcode.h>
#include <module/motion.h>
#include <module/probe.h>
#include <module/temperature.h>
#include <module/planner.h>
#include <module/stepper.h>
#include <module/signal2step.hpp>
#include <feature/pressure_advance/pressure_advance_config.hpp>
#include <feature/phase_stepping/phase_stepping.hpp>
#include <feature/precise_stepping/manual.hpp>
#include <signal_processing/analysis.hpp>
#include <signal_processing/generators.hpp>
#include <signal_processing/pipeline.hpp>
#include <signal_processing/filters.hpp>
#include "loadcell.hpp"

#include <logging/log.hpp>
#include <raii/scope_guard.hpp>
#include <raii/auto_restore.hpp>
#include <timing.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <sfl/segmented_vector.hpp>
#include <puppies/INDX.hpp>
#include <puppies/PuppyModbus.hpp>
#include <printers.h>
#include <cstdint>

LOG_COMPONENT_DEF(ContactlessOffset, logging::Severity::debug);

// Debugging info is disabled by default
// In order to use debug_tool_offset.py script, set TOOL_OFFSET_DEBUG() to 1
#define TOOL_OFFSET_DEBUG() 0

// Margin (in samples) added around each chunk to suppress filter edge artifacts.
static constexpr size_t preprocess_edge_margin = 10;

static constexpr uint8_t max_retries = 3;
static constexpr float peak_width_mm = 0.6f;
static constexpr float high_confidence_threshold = 0.9f;

struct EnergyRegion {
    size_t start;
    size_t end;
};

struct LineMotionConfig {
    xy_pos_t start;
    xy_pos_t end;
    float speed = 7.0f;
    float speed2 = 15.0f;
    float accel = 2000.0f;
    float rest_time = 0.0f;
    float symmetry_trim_fraction;
};

struct SymmetryPeakResult {
    float peak_time_s;
    float confidence;
    float correlation_peak;
    // Pass-1 (full signal) correlation peak. Equals correlation_peak when no
    // tail trimming is performed; otherwise the pre-trim score for diagnostics.
    float correlation_peak_full = 0.0f;
};

struct FourPassPeaks {
    SymmetryPeakResult pass1, pass2, pass3, pass4;
};

struct PositionEstimate {
    float position_mm;
    float time_offset_s;
    float residual_variance;
};

struct TwoSpeedAnalysisResult {
    FourPassPeaks peaks;
    PositionEstimate estimate_all;
    PositionEstimate estimate_forward;
    PositionEstimate estimate_backward;
    float confidence;

    float delta_12_obs, delta_34_obs;
    float delta_13_obs, delta_24_obs;
    float delta_12_model, delta_34_model;
};

// Two-speed sweep motion profile.
// Pure configuration — all derived quantities are computed on demand.
struct SweepSpeedProfile {
    float rest_time;
    float accel;
    float speed1;
    float speed2;
    float total_distance;

    static SweepSpeedProfile from_config(const LineMotionConfig &config, float total_distance) {
        return { config.rest_time, config.accel, config.speed, config.speed2, total_distance };
    }

    // Computed timing for a single trapezoidal pass at a given speed
    static float compute_pass_time(float speed, float accel, float distance) {
        if (speed <= 0 || accel <= 0 || distance <= 0) {
            return 0;
        }
        float accel_time = speed / accel;
        float accel_distance = 0.5f * accel * accel_time * accel_time;
        if (2.0f * accel_distance >= distance) {
            return 2.0f * std::sqrt(distance / accel);
        }
        return 2.0f * accel_time + (distance - 2.0f * accel_distance) / speed;
    }

    float pass_time1() const { return compute_pass_time(speed1, accel, total_distance); }
    float pass_time2() const { return compute_pass_time(speed2, accel, total_distance); }
    float total_time() const { return 5.0f * rest_time + 2.0f * pass_time1() + 2.0f * pass_time2(); }

    size_t total_samples(sp::SamplingFreq freq) const {
        return static_cast<size_t>(total_time() * freq);
    }

    std::array<float, 4> expected_peak_times() const {
        float r = rest_time;
        float pt1 = pass_time1();
        float pt2 = pass_time2();
        return {
            r + pt1 / 2.0f,
            r + pt1 + r + pt1 / 2.0f,
            2.0f * r + 2.0f * pt1 + r + pt2 / 2.0f,
            2.0f * r + 2.0f * pt1 + r + pt2 + r + pt2 / 2.0f,
        };
    }

    // Build a type-erased velocity source for the full sweep motion
    sp::pipe::SignalSource<float> make_source(sp::SamplingFreq freq) const {
        auto trapezoid = [&](float speed) -> sp::pipe::SignalSource<float> {
            float accel_time = speed / accel;
            float accel_dist = 0.5f * accel * accel_time * accel_time;
            float cruise_speed = speed;
            if (2.0f * accel_dist >= total_distance) {
                accel_dist = total_distance / 2.0f;
                accel_time = std::sqrt(2.0f * accel_dist / accel);
                cruise_speed = accel * accel_time;
            }
            float cruise_dist = total_distance - 2.0f * accel_dist;
            float cruise_time = (cruise_speed > 0.0f) ? cruise_dist / cruise_speed : 0.0f;
            int accel_samples = static_cast<int>(accel_time * freq);
            int cruise_samples = static_cast<int>(cruise_time * freq);
            return sp::pipe::chain(
                sp::Ramp<float>(0.0f, accel, freq) | sp::pipe::take_samples(accel_samples),
                sp::pipe::make_constant(cruise_speed, freq) | sp::pipe::take_samples(cruise_samples),
                sp::Ramp<float>(cruise_speed, -accel, freq) | sp::pipe::take_samples(accel_samples));
        };
        auto rest = [&]() -> sp::pipe::SignalSource<float> {
            return sp::pipe::make_constant(0.0f, freq)
                | sp::pipe::take_samples(static_cast<int>(rest_time * freq));
        };
        auto backward_trapezoid = [&](float speed) {
            return trapezoid(speed) | sp::pipe::transform([](float v) { return -v; });
        };

        return sp::pipe::chain(
            rest(),
            trapezoid(speed1),
            rest(),
            backward_trapezoid(speed1),
            rest(),
            trapezoid(speed2),
            rest(),
            backward_trapezoid(speed2),
            rest());
    }
};

struct RawRecordedSample {
    uint32_t timestamp_us;
    float sensor_value;
};

struct MotionExecutionResult {
    sfl::segmented_vector<RawRecordedSample, 512> raw_samples;
    abce_pos_t moved_by;
    float sensor_sampling_freq_hz;
    uint32_t motion_profile_start_us;

    uint32_t convert_time_ms;
    uint32_t steps_time_ms;
    uint32_t expected_time_ms;
    uint32_t actual_time_ms;
};

// We place the debug reporters into a separate file so we do not clutter
// the business logic with debug-only code.
#include "debug_reporters.ipp"

static std::expected<TwoSpeedAnalysisResult, const char *> execute_and_analyze_sweep(
    const LineMotionConfig &config,
    tool_offset::Sensor &sensor,
    const char *label);

static constexpr float position_tolerance = 0.01f;

static float measure_sensor_true_z(const tool_offset::ProbingConfig &config) {
    assert(std::abs(current_position.x - config.sensor_position.x) < position_tolerance);
    assert(std::abs(current_position.y - config.sensor_position.y) < position_tolerance);

    // Both are needed to run `probe_here`
    pressure_advance::PressureAdvanceDisabler pa_disabler;
    Loadcell::HighPrecisionEnabler loadcell_high_precision_enabler(loadcell);

    const float probed_z = probe_here(config.sensor_position.z);
    return probed_z;
}

static bool wait_for_first_sample(tool_offset::Sensor &sensor, uint32_t timeout_us = 2'000'000) {
    uint32_t start = ticks_us();
    while (!sensor.get_sample().has_value()) {
        if (sensor.get_last_error() != tool_offset::Sensor::Error::NONE) {
            return false;
        }
        if (ticks_diff(ticks_us(), start) > static_cast<int32_t>(timeout_us)) {
            return false;
        }
        idle(true);
    }
    return true;
}

// Execute motion while recording sensor samples
template <typename MotionSignal>

std::expected<MotionExecutionResult, const char *> execute_motion_with_recording(
    MotionSignal &&motion_signal,
    tool_offset::Sensor &sensor,
    const char *label,
    uint32_t expected_duration_us) {

    MotionExecutionResult result = {};

    // Get mm per step from planner
    for (int ax = 0; ax < XYZE; ++ax) {
        if (planner.settings.axis_steps_per_mm[ax] <= 0) {
            result.raw_samples.clear();
            return std::unexpected<const char *>("Invalid axis steps per mm");
        }
    }
    abce_pos_t mm_per_step;
    mm_per_step.pos[0] = 1.0f / planner.settings.axis_steps_per_mm[X_AXIS];
    mm_per_step.pos[1] = 1.0f / planner.settings.axis_steps_per_mm[Y_AXIS];
    mm_per_step.pos[2] = 1.0f / planner.settings.axis_steps_per_mm[Z_AXIS];
    mm_per_step.pos[3] = 1.0f / planner.settings.axis_steps_per_mm[E_AXIS];

    LineSamplesDebugReporter samples_reporter(label);
    samples_reporter.start();

    sensor.start();
    if (!wait_for_first_sample(sensor)) {
        sensor.stop();
        return std::unexpected<const char *>("Failed to get first sensor sample");
    }

    enable_all_steppers();

    auto receive_samples = [&]() {
        while (auto sample = sensor.get_sample()) {
            uint32_t now = ticks_us();
            result.raw_samples.push_back({ now, *sample });
            samples_reporter.report_sample(*sample);
        }
    };

    auto enqueue_step = [&, prev_ts = uint32_t { 0 }](uint32_t timestamp_us, AxisEnum axis, bool direction) mutable {
        // Always drain sensor queue to prevent overflow — idle() can be slow during a print
        receive_samples();

        // Enqueue to precise stepping, polling sensor while waiting
        while (precise_stepping::manual::is_full()) {
            receive_samples();
            idle(true);
        }

        // Calculate relative timestamp
        uint32_t relative_ts = timestamp_us - prev_ts;
        while (relative_ts > STEP_TIMER_MAX_TICKS_LIMIT) {
            precise_stepping::manual::enqueue_step(STEP_TIMER_MAX_TICKS_LIMIT, false,
                StepEventFlag::STEP_EVENT_FLAG_KEEP_ALIVE);
            relative_ts -= STEP_TIMER_MAX_TICKS_LIMIT;
        }

        StepEventFlag_t flags = StepEventFlag::STEP_EVENT_FLAG_STEP_X << int(axis)
            | StepEventFlag::STEP_EVENT_FLAG_X_ACTIVE << int(axis);
        precise_stepping::manual::enqueue_step(relative_ts, !direction, flags);
        prev_ts = timestamp_us;
    };

    result.expected_time_ms = expected_duration_us / 1000;

    {
        phase_stepping::EnsureDisabled _;

        uint32_t motion_start_time_us = ticks_us();

        result.moved_by = signal2step::convert(std::forward<MotionSignal>(motion_signal), mm_per_step, enqueue_step);

        result.motion_profile_start_us = motion_start_time_us;

        result.convert_time_ms = (ticks_us() - motion_start_time_us) / 1000;

        // Wait for all steps to complete while continuing to poll sensor
        while (precise_stepping::manual::has_steps()) {
            receive_samples();
            idle(true);
        }

        result.steps_time_ms = (ticks_us() - motion_start_time_us) / 1000;

        // Continue polling sensor until expected motion duration has elapsed
        if (expected_duration_us > 0) {
            while ((ticks_us() - motion_start_time_us) < expected_duration_us) {
                receive_samples();
                idle(true);
            }
        }

        result.actual_time_ms = (ticks_us() - motion_start_time_us) / 1000;

        // Update current position
        xyze_pos_t new_pos = current_position + signal2step::from_machine_to_cartesian(result.moved_by);
        current_position = new_pos;
        destination = new_pos;
        sync_plan_position();
        PreciseStepping::reset_from_halt(false);
    }

    // Get sampling frequency before stopping the sensor
    result.sensor_sampling_freq_hz = sensor.sampling_freq();
    auto error = sensor.get_last_error();
    sensor.stop();
    samples_reporter.finish(result.sensor_sampling_freq_hz);

    switch (error) {
    case tool_offset::Sensor::Error::HW_FAILURE:
        log_error(ContactlessOffset, "Sensor reported hardware failure");
        return std::unexpected<const char *>("Sensor reported hardware failure");
        break;
    case tool_offset::Sensor::Error::OVERFLOW:
        log_error(ContactlessOffset, "Sensor sample overflow");
        return std::unexpected<const char *>("Sensor samples overflow");
    case tool_offset::Sensor::Error::NONE:
        break;
    }
    return std::expected<MotionExecutionResult, const char *>(result);
}

// Project speed sources along (dir_x, dir_y) into XYZE motion signal
template <typename SpeedSourceX, typename SpeedSourceY>
static auto create_motion_signal(
    SpeedSourceX &speed_source_x,
    SpeedSourceY &speed_source_y,
    float dir_x,
    float dir_y,
    size_t num_samples,
    sp::SamplingFreq sampling_freq) {

    return signal2step::fuse_xyze(
               sp::pipe::ref(speed_source_x) | sp::pipe::integrate(0.0f) | sp::pipe::transform([dir_x](float pos) { return pos * dir_x; }),
               sp::pipe::ref(speed_source_y) | sp::pipe::integrate(0.0f) | sp::pipe::transform([dir_y](float pos) { return pos * dir_y; }),
               sp::pipe::make_constant(0.0f, sampling_freq) | sp::pipe::take_samples(static_cast<int>(num_samples)),
               sp::pipe::make_constant(0.0f, sampling_freq) | sp::pipe::take_samples(static_cast<int>(num_samples)))
        | signal2step::cartesian_to_printer_kinematics();
}

namespace {

// Measurement orchestration is structured as a small finite state machine.
// Each scan can produce one of three outcomes (Event), and the next_state
// run the action function that performs the transition.
enum class FsmState : size_t {
    offset_measurement_x,
    offset_measurement_y,
    offset_measurement_x_when_y_confident,
    offset_measurement_y_when_x_confident,
    finished,
};

enum class FsmEvent : size_t {
    measurement_failed,
    measurement_low_confidence,
    measurement_high_confidence,
};

struct ScanOutcome {
    float offset;
    float confidence;
    FsmEvent event;
};

struct ScanParams {
    const tool_offset::ProbingConfig &config;
    tool_offset::Sensor &sensor;
    const char *name;
    bool along_x;
    float center;
    float cross_pos;
};

ScanOutcome run_scan(const ScanParams &p) {
    const float scan_half_width = p.along_x ? p.config.sensing_distance_x / 2.0f : p.config.sensing_distance_y / 2.0f;
    const float scan_start = p.center - scan_half_width;
    const float scan_end = p.center + scan_half_width;

    // Before move, check all physical limits to avoid crashes
    if (p.along_x) {
        // On C1_INDX we are also limited by the bed sheet
        if (scan_start < (X_BED_SIZE + peak_width_mm) || scan_end > X_MAX_POS) {
            log_error(ContactlessOffset, "Scan along X axis exceeds physical limits");
            return { 0.0f, 0.0f, FsmEvent::measurement_low_confidence }; // dont' return failed to allow retry with different center position
        }
    } else {
        if (scan_start < Y_MIN_POS || scan_end > Y_MAX_POS) {
            log_error(ContactlessOffset, "Scan along Y axis exceeds physical limits");
            return { 0.0f, 0.0f, FsmEvent::measurement_low_confidence }; // dont' return failed to allow retry with different center position
        }
    }

    LineMotionConfig cfg;
    if (p.along_x) {
        cfg.start.set(scan_start, p.cross_pos);
        cfg.end.set(scan_end, p.cross_pos);
    } else {
        cfg.start.set(p.cross_pos, scan_start);
        cfg.end.set(p.cross_pos, scan_end);
    }
    cfg.speed = p.config.sensing_speed_slow;
    cfg.speed2 = p.config.sensing_speed_fast;
    cfg.rest_time = p.config.sweep_rest_time;
    cfg.symmetry_trim_fraction = p.config.symmetry_trim_fraction;

    debug_report_scan_start(p.name);
    auto scan_result = execute_and_analyze_sweep(cfg, p.sensor, p.name);
    if (!scan_result.has_value()) {
        log_error(ContactlessOffset, "scan '%s' failed: %s", p.name, scan_result.error());
        return { 0.0f, 0.0f, FsmEvent::measurement_failed };
    }
    const float offset = scan_half_width - scan_result->estimate_all.position_mm;
    const float confidence = scan_result->confidence;
    debug_report_scan_result(p.name, confidence, offset);

    const FsmEvent event = (confidence < high_confidence_threshold)
        ? FsmEvent::measurement_low_confidence
        : FsmEvent::measurement_high_confidence;
    return { offset, confidence, event };
}

// Per-run state shared across FSM iterations. The dispatcher updates the
// cross-axis position cache (offset_for_measurement.x/y) so each scan passes
// through the strongest part of the sensor field discovered by the previous
// scan, and tracks retry counters used by the FSM actions.
//
// `scan_center` is the sensor position clamped to physical reach (so the scan
// sweep stays within machine limits). It may differ from
// `config.sensor_position` when the sensor sits near the edge of travel — the
// caller compensates for that delta when interpreting the FSM result.
struct FsmContext {
    const tool_offset::ProbingConfig &config;
    tool_offset::Sensor &sensor;
    xy_pos_t scan_center;
    xy_pos_t offset_for_measurement {};
    uint8_t generic_retry_count {};
    uint8_t scan_count_y {};
    ScanOutcome om_x {};
    ScanOutcome om_y {};
};

// Per-state scan launchers. Each builds the ScanParams for its state, runs
// the scan, stashes the outcome into the context, and (for center-detection
// passes) updates the cross-axis position cache. Hard errors are reported by
// run_scan as FsmEvent::measurement_failed; in that case we skip the cache
// update so a transient failure can't poison the next iteration's geometry.
ScanOutcome scan_offset_measurement_x(FsmContext &ctx) {
    auto r = run_scan({
        .config = ctx.config,
        .sensor = ctx.sensor,
        .name = "nozzle-offset-x",
        .along_x = true,
        .center = ctx.scan_center.x,
        .cross_pos = ctx.scan_center.y - ctx.offset_for_measurement.y,
    });
    if (r.event == FsmEvent::measurement_high_confidence) {
        ctx.offset_for_measurement.x = r.offset;
    }
    return r;
}

ScanOutcome scan_offset_measurement_y(FsmContext &ctx) {
    auto r = run_scan({
        .config = ctx.config,
        .sensor = ctx.sensor,
        .name = "nozzle-offset-y",
        .along_x = false,
        .center = ctx.scan_center.y,
        .cross_pos = ctx.scan_center.x - ctx.offset_for_measurement.x,
    });
    if (r.event == FsmEvent::measurement_high_confidence) {
        ctx.offset_for_measurement.y = r.offset;
    }
    return r;
}

// Transition rule: given (current state, scan outcome), produce the next state.
//
// Overall arc: start with an X scan; once one axis is confident, refine the
// other in the matching _when_*_confident state, which uses the known axis as
// its cross-axis reference. Specifics per state are commented at the case.
//
// Execution failures arrive as measurement_failed;
// geometric limit violations are intentionally surfaced as measurement_low_confidence
// so the cross-axis hunt can move them back into range.
FsmState next_state(FsmState state, FsmEvent event, FsmContext &ctx) {
    auto proceed = [&](FsmState target) {
        ctx.generic_retry_count = 0;
        ctx.scan_count_y = 0;
        return target;
    };

    auto helper_retry = [&](FsmState target, const char axis) -> FsmState {
        if (++ctx.generic_retry_count > max_retries) {
            log_error(ContactlessOffset, "%c failed after %d retries, giving up", axis, max_retries);
            return FsmState::finished;
        }
        log_warning(ContactlessOffset, "%c failed, retrying (%d/%d)", axis, ctx.generic_retry_count, max_retries);
        return target;
    };
    // Retry an X scan, capped at max_retries
    auto retry_x = [&](FsmState target) -> FsmState {
        return helper_retry(target, 'X');
    };

    // Plain retry of a Y scan after a hard failure, capped at max_retries.
    auto retry_y_on_error = [&](FsmState target) -> FsmState {
        return helper_retry(target, 'Y');
    };

    // Low-confidence Y: sensor probably isn't under the swept line. Step the
    // cross-axis X across sensing_distance_x and rescan; up to max_count_y tries.
    constexpr uint8_t max_count_y = 10;
    auto retry_y_low_conf = [&](FsmState target) -> FsmState {
        if (ctx.scan_count_y > max_count_y) {
            return FsmState::finished;
        }
        const float x_offset_step = ctx.config.sensing_distance_x / max_count_y;
        ctx.offset_for_measurement.x = ctx.scan_count_y * x_offset_step - ctx.config.sensing_distance_x / 2;
        ctx.scan_count_y++;
        log_warning(ContactlessOffset, "Y not confident, retrying (%d/%d)", ctx.scan_count_y, max_count_y);
        return target;
    };

    switch (state) {
    case FsmState::offset_measurement_x:
        // Initial X. Low confidence skips ahead to Y — Y has the longer sweep
        // and a wider chance of locating the sensor; we return to refine X once
        // Y is confident.
        // Hard failure retries X.
        switch (event) {
        case FsmEvent::measurement_failed:
            return retry_x(FsmState::offset_measurement_x);
        case FsmEvent::measurement_low_confidence:
            return proceed(FsmState::offset_measurement_y);
        case FsmEvent::measurement_high_confidence:
            return proceed(FsmState::offset_measurement_y_when_x_confident);
        }
        break;

    case FsmState::offset_measurement_y:
        // Initial Y. Low confidence means we're off-sensor in X
        // Hunt by stepping the cross-axis X.
        // Success: refine X using this Y.
        // Hard failure: plain retry.
        switch (event) {
        case FsmEvent::measurement_failed:
            return retry_y_on_error(FsmState::offset_measurement_y);
        case FsmEvent::measurement_low_confidence:
            return retry_y_low_conf(FsmState::offset_measurement_y);
        case FsmEvent::measurement_high_confidence:
            return proceed(FsmState::offset_measurement_x_when_y_confident);
        }
        break;

    case FsmState::offset_measurement_x_when_y_confident:
        // Refinement pass with known Y. Retry on either weak outcome (Y stays
        // pinned as confident); finish on success.
        switch (event) {
        case FsmEvent::measurement_failed:
            return retry_x(FsmState::offset_measurement_x_when_y_confident);
        case FsmEvent::measurement_low_confidence:
            return retry_x(FsmState::offset_measurement_x_when_y_confident);
        case FsmEvent::measurement_high_confidence:
            return proceed(FsmState::finished);
        }
        break;

    case FsmState::offset_measurement_y_when_x_confident:
        // Refinement pass with known X. Retry on either weak outcome (Y probably just needs a nudge); finish on success.
        switch (event) {
        case FsmEvent::measurement_failed:
            return retry_y_on_error(FsmState::offset_measurement_y_when_x_confident);
        case FsmEvent::measurement_low_confidence:
            return retry_y_on_error(FsmState::offset_measurement_y_when_x_confident);
        case FsmEvent::measurement_high_confidence:
            return proceed(FsmState::finished);
        }
        break;

    case FsmState::finished:
        break;
    }
    return FsmState::finished; // unreachable
}

// Drive the FSM. Each iteration: run the scan for the current state, classify
// its outcome, ask next_state() for the next state, transition. Terminates
// when next_state() returns FsmState::finished. Hard errors from run_scan are
// logged at the source and surfaced as FsmEvent::measurement_failed; the
// dispatcher just retries (or eventually hits the iteration cap below).
// Returns nullptr on success, or an error string on failure.
const char *dispatch_fsm(FsmContext &ctx) {
    constexpr unsigned max_iterations = 14;

    FsmState state = FsmState::offset_measurement_x;
    for (unsigned i = 0; i < max_iterations; ++i) {
        if (state == FsmState::finished && ctx.om_x.confidence > high_confidence_threshold && ctx.om_y.confidence > high_confidence_threshold) {
            return nullptr;
        } else if (state == FsmState::finished) {
            return "Tool offset FSM finished without high confidence in both axes";
        }

        ScanOutcome outcome;
        switch (state) {
        case FsmState::offset_measurement_x:
        case FsmState::offset_measurement_x_when_y_confident:
            outcome = scan_offset_measurement_x(ctx);
            ctx.om_x = outcome;
            break;
        case FsmState::offset_measurement_y:
        case FsmState::offset_measurement_y_when_x_confident:
            outcome = scan_offset_measurement_y(ctx);
            ctx.om_y = outcome;
            break;
        default:
            break;
        };

        state = next_state(state, outcome.event, ctx);
    }
    return "Tool offset FSM exceeded iteration limit"; // unreachable
}

// RAII guard that disables the INDX loadcell + accelerometer and zeros the
// hotend/bed targets for the duration of an XY scan, restoring everything on
// destruction. Disabling the puppy traffic frees Modbus bandwidth for the
// tool-offset sensor stream; zeroing the heaters reduces electromagnetic noise on
// the induction tool-offset sensor. Must only be constructed AFTER the Z probe,
// since the loadcell is needed for that probe.
class IndxScanState {
    Hotend &hotend_;
    bool prev_loadcell_active_;
    bool prev_accelerometer_active_;
    int16_t prev_hotend_target_;
    int16_t prev_bed_target_;

public:
    explicit IndxScanState(Hotend &hotend)
        : hotend_(hotend)
        , prev_loadcell_active_(buddy::puppies::indx.get_loadcell_active())
        , prev_accelerometer_active_(buddy::puppies::indx.get_accelerometer_active())
        , prev_hotend_target_(hotend.nozzle_target_temp())
        , prev_bed_target_(thermalManager.degTargetBed()) {
        buddy::puppies::indx.set_loadcell(buddy::puppies::puppyModbus, false);
        buddy::puppies::indx.set_accelerometer(buddy::puppies::puppyModbus, false);
        hotend_.set_nozzle_target_temp(0);
        thermalManager.setTargetBed(0);
    }

    ~IndxScanState() {
        buddy::puppies::indx.set_loadcell(buddy::puppies::puppyModbus, prev_loadcell_active_);
        buddy::puppies::indx.set_accelerometer(buddy::puppies::puppyModbus, prev_accelerometer_active_);
        hotend_.set_nozzle_target_temp(prev_hotend_target_);
        thermalManager.setTargetBed(prev_bed_target_);
    }

    IndxScanState(const IndxScanState &) = delete;
    IndxScanState &operator=(const IndxScanState &) = delete;
};

// Move above the sensor and probe down to find its true Z. Leaves the
// carriage at the probed Z (loadcell remains active so the caller can
// reposition before disabling it).
std::expected<float, const char *> probe_sensor_z(const tool_offset::ProbingConfig &config) {
    do_blocking_move_to_z(config.sensor_position.z + config.safe_z_height);

    // Z-probing over TOS must be done out of the coil area to avoid destruction of the coil
    auto z_probing_position = config.sensor_position;
    z_probing_position.y += config.y_shift_z_probe_offset_from_sensor;
    do_blocking_move_to_xy(z_probing_position);

    const float sensor_z = measure_sensor_true_z(config);
    if (std::isnan(sensor_z)) {
        return std::unexpected("Initial probing failed, sensor Z is NaN");
    }
    return sensor_z;
}

// Run the XY-scan FSM. Assumes the carriage is already positioned at the
// sensor XY at the desired probing Z. Returns the measured (x, y) offset in
// the caller's frame (i.e. relative to `config.sensor_position`).
std::expected<xy_pos_t, const char *> measure_xy_via_fsm(
    const tool_offset::ProbingConfig &config,
    tool_offset::Sensor &sensor,
    const tool_offset::ToolOffset &initial_measurement_offset) {

    auto &hotend = Hotend::for_tool(PhysicalToolIndex::currently_selected());
    IndxScanState scan_state(hotend);

    // Clamp the scan center to physical reach so the sweep stays within
    // machine limits even when the sensor sits close to an edge. The scan
    // remains symmetric around `scan_center`, so any difference from
    // `config.sensor_position` shifts every reported offset by the same delta;
    // we undo that shift on the result below.
    const xy_pos_t scan_center { { {
        std::clamp(
            config.sensor_position.x,
            X_MIN_POS + config.sensing_distance_x / 2.0f + X_MAX_OFFSET,
            X_MAX_POS - config.sensing_distance_x / 2.0f - X_MAX_OFFSET),
        std::clamp(
            config.sensor_position.y,
            Y_MIN_POS + config.sensing_distance_y / 2.0f + Y_MAX_OFFSET,
            Y_MAX_POS - config.sensing_distance_y / 2.0f - Y_MAX_OFFSET),
    } } };
    const xy_pos_t scan_center_delta { { {
        scan_center.x - config.sensor_position.x,
        scan_center.y - config.sensor_position.y,
    } } };

    // Seed the cross-axis position cache with the caller's estimate (clamped
    // to the scan range so it can't bias the cross-axis outside the sensor
    // area). The seed is expressed in the scan_center frame, so the same
    // delta is folded in here too.
    const float clamp_x_lo = peak_width_mm - config.sensing_distance_x / 2.0f;
    const float clamp_x_hi = peak_width_mm + config.sensing_distance_x / 2.0f;
    const float clamp_y_lo = peak_width_mm - config.sensing_distance_y / 2.0f;
    const float clamp_y_hi = peak_width_mm + config.sensing_distance_y / 2.0f;

    FsmContext ctx {
        .config = config,
        .sensor = sensor,
        .scan_center = scan_center,
        .offset_for_measurement = { { {
            std::clamp(initial_measurement_offset.x, clamp_x_lo, clamp_x_hi) + scan_center_delta.x,
            std::clamp(initial_measurement_offset.y, clamp_y_lo, clamp_y_hi) + scan_center_delta.y,
        } } },
    };

    if (const char *err = dispatch_fsm(ctx); err != nullptr) {
        return std::unexpected(err);
    }

    // Undo the scan_center shift so the result is the true offset between the
    // nozzle and the configured sensor position.
    return xy_pos_t { { {
        ctx.om_x.offset - scan_center_delta.x,
        ctx.om_y.offset - scan_center_delta.y,
    } } };
}

} // namespace

std::expected<tool_offset::ToolOffset, const char *> tool_offset::measure_current_tool_offset(
    const tool_offset::ProbingConfig &config,
    tool_offset::Sensor &sensor,
    const tool_offset::ToolOffset &initial_measurement_offset) {

    auto &hotend = Hotend::for_tool(PhysicalToolIndex::currently_selected());

    // Check nozzle temperature before probing
    if (hotend.nozzle_temp() > config.max_safe_temp) {
        return std::unexpected("Nozzle too hot for probing");
    }

    if (!GcodeSuite::G28_no_parser(true, true, true, G28Flags { .only_if_needed = true })) {
        return std::unexpected("Homing failed");
    }

    // Sensor may be below soft endstop limits — disable them for the whole measurement
    AutoRestore restore_soft_endstops(soft_endstops_enabled, false);

    const auto sensor_z = probe_sensor_z(config);
    if (!sensor_z) {
        return std::unexpected(sensor_z.error());
    }

    do_blocking_move_to_z(*sensor_z + config.sensing_z);
    debug_report_probed_z(*sensor_z, *sensor_z - config.sensor_position.z);

    const auto xy = measure_xy_via_fsm(config, sensor, initial_measurement_offset);
    if (!xy) {
        return std::unexpected(xy.error());
    }

    return tool_offset::ToolOffset {
        .x = xy->x,
        .y = xy->y,
        .z = *sensor_z - config.sensor_position.z,
    };
}

// Signal preprocessing: median filter + normalize + zero-phase lowpass + detrend
// Returns false if signal has no variance (flat).
static bool preprocess_signal(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    size_t chunk_start_idx,
    size_t chunk_size,
    float dt,
    sfl::segmented_vector<float, 256> &signal_value) {

    // Extend the processing range by a margin on each side to avoid
    // edge artifacts from the median and lowpass filters. The margin
    // is trimmed from the output at the end.
    const size_t margin_before = std::min(preprocess_edge_margin, chunk_start_idx);
    const size_t margin_after = std::min(preprocess_edge_margin, raw_samples.size() - (chunk_start_idx + chunk_size));
    const size_t ext_start = chunk_start_idx - margin_before;
    const size_t ext_size = margin_before + chunk_size + margin_after;

    // Zero-phase median filter: forward + backward, averaged.
    // A causal median filter shifts the symmetry axis due to asymmetric
    // startup transients. Averaging forward and backward passes cancels this.
    sp::MedianFilter<float, 5> median_filter;

    signal_value.reserve(ext_size);
    for (size_t i = 0; i < ext_size; ++i) {
        signal_value.push_back(median_filter.filter(raw_samples[ext_start + i].sensor_value));
    }

    sfl::segmented_vector<float, 256> bwd;
    bwd.reserve(ext_size);
    bwd.resize(ext_size);
    median_filter.reset();
    for (size_t i = ext_size; i-- > 0;) {
        bwd[i] = median_filter.filter(raw_samples[ext_start + i].sensor_value);
    }

    for (size_t i = 0; i < ext_size; ++i) {
        signal_value[i] = (signal_value[i] + bwd[i]) * 0.5f;
    }

    auto [mean, std_val] = sp::normalize_inplace(signal_value);
    if (std_val < 1e-10f) {
        return false;
    }

    constexpr float signal_lowpass_cutoff_hz = 50.0f;
    auto lowpass_coeffs = sp::butterworth_lowpass_biquad_2nd(signal_lowpass_cutoff_hz, 1.0f / dt);
    sp::zero_phase_biquad_padded<sfl::segmented_vector<float, 256>>(
        signal_value, lowpass_coeffs, 1.0f / dt, signal_lowpass_cutoff_hz);

    sp::linear_detrend(signal_value);

    // Trim margins to return only the original chunk range
    if (margin_after > 0) {
        signal_value.erase(signal_value.end() - margin_after, signal_value.end());
    }
    if (margin_before > 0) {
        signal_value.erase(signal_value.begin(), signal_value.begin() + margin_before);
    }
    return true;
}

// Compute finite differences and normalize to unit variance
static void compute_signal_derivative(
    const sfl::segmented_vector<float, 256> &signal_value,
    sfl::segmented_vector<float, 256> &signal_deriv) {

    size_t n = signal_value.size();
    if (n < 2) {
        return;
    }

    signal_deriv.reserve(n - 1);
    for (size_t i = 1; i < n; ++i) {
        signal_deriv.push_back(signal_value[i] - signal_value[i - 1]);
    }

    sp::normalize_inplace(signal_deriv);
}

// Evaluate normalized combined (value + derivative) correlation score at a lag.
static float combined_correlation_score(
    const sfl::segmented_vector<float, 256> &signal_value,
    const sfl::segmented_vector<float, 256> &signal_deriv,
    int lag,
    float inv_val,
    float inv_der) {
    float cv = sp::symmetry_correlation(signal_value, lag, 1.0f) * inv_val;
    float cd = sp::symmetry_correlation(signal_deriv, lag, -1.0f) * inv_der;
    return cv + cd;
}

// Search a lag range for the best combined (value+derivative) correlation.
template <typename ValCorrFn, typename DerCorrFn>
static void find_best_correlation_lag(
    int lag_min, int lag_max,
    ValCorrFn value_corr_fn, DerCorrFn deriv_corr_fn,
    int &out_best_lag, float &out_best_combined) {

    // Find normalization factors
    float max_abs_val = 0, max_abs_der = 0;
    for (int lag = lag_min; lag <= lag_max; ++lag) {
        max_abs_val = std::max(max_abs_val, std::abs(value_corr_fn(lag)));
        max_abs_der = std::max(max_abs_der, std::abs(deriv_corr_fn(lag)));
    }

    float inv_val = (max_abs_val > 1e-10f) ? 1.0f / max_abs_val : 0.0f;
    float inv_der = (max_abs_der > 1e-10f) ? 1.0f / max_abs_der : 0.0f;

    // Find peak of normalized combined score
    out_best_combined = -std::numeric_limits<float>::infinity();
    out_best_lag = lag_min;
    for (int lag = lag_min; lag <= lag_max; ++lag) {
        float combined = value_corr_fn(lag) * inv_val + deriv_corr_fn(lag) * inv_der;
        if (combined > out_best_combined) {
            out_best_combined = combined;
            out_best_lag = lag;
        }
    }
}

// Coarse-to-fine symmetry correlation search.
// Returns the best lag (in samples) and combined correlation score.
static int find_approx_symmetry_lag(
    const sfl::segmented_vector<float, 256> &signal_value,
    const sfl::segmented_vector<float, 256> &signal_deriv,
    size_t max_lag,
    float &out_best_combined,
    int &out_fine_min,
    int &out_fine_max) {

    int best_lag = 0;
    out_best_combined = 0;

    auto value_corr = [&](int lag) { return sp::symmetry_correlation(signal_value, lag, 1.0f); };
    auto deriv_corr = [&](int lag) { return sp::symmetry_correlation(signal_deriv, lag, -1.0f); };

    constexpr size_t decimate_factor = 8;

    // Downsample with averaging for coarse search
    sfl::segmented_vector<float, 256> val_ds, der_ds;
    sp::decimate_average(signal_value, val_ds, decimate_factor);
    sp::decimate_average(signal_deriv, der_ds, decimate_factor);

    auto ds_val_corr = [&](int lag) { return sp::symmetry_correlation(val_ds, lag, 1.0f); };
    auto ds_der_corr = [&](int lag) { return sp::symmetry_correlation(der_ds, lag, -1.0f); };

    int max_lag_ds = static_cast<int>(max_lag / decimate_factor);
    int coarse_best = 0;
    float coarse_combined = 0;
    find_best_correlation_lag(-max_lag_ds, max_lag_ds, ds_val_corr, ds_der_corr, coarse_best, coarse_combined);

    // Fine pass: ±2 coarse bins around coarse result
    int fine_center = coarse_best * static_cast<int>(decimate_factor);
    int fine_radius = static_cast<int>(2 * decimate_factor);
    int fine_min = std::max(fine_center - fine_radius, -static_cast<int>(max_lag));
    int fine_max = std::min(fine_center + fine_radius, static_cast<int>(max_lag));
    out_fine_min = fine_min;
    out_fine_max = fine_max;

    find_best_correlation_lag(fine_min, fine_max, value_corr, deriv_corr, best_lag, out_best_combined);

    debug_report_symmetry_search(
        signal_value, signal_deriv, static_cast<unsigned>(max_lag),
        coarse_best, coarse_combined,
        fine_min, fine_max, best_lag, out_best_combined);

    return best_lag;
}

// Sub-sample refinement of the symmetry lag via parabolic interpolation
static float refine_lag_parabolic(
    const sfl::segmented_vector<float, 256> &signal_value,
    const sfl::segmented_vector<float, 256> &signal_deriv,
    int best_lag, size_t max_lag) {

    int lag_m1 = best_lag - 1;
    int lag_p1 = best_lag + 1;
    if (lag_m1 < -static_cast<int>(max_lag) || lag_p1 > static_cast<int>(max_lag)) {
        return static_cast<float>(best_lag);
    }

    // Compute normalization factors from the 3-point neighborhood
    float max_abs_val = 0, max_abs_der = 0;
    for (int lag : { lag_m1, best_lag, lag_p1 }) {
        max_abs_val = std::max(max_abs_val, std::abs(sp::symmetry_correlation(signal_value, lag, 1.0f)));
        max_abs_der = std::max(max_abs_der, std::abs(sp::symmetry_correlation(signal_deriv, lag, -1.0f)));
    }
    float iv = (max_abs_val > 1e-10f) ? 1.0f / max_abs_val : 0.0f;
    float id = (max_abs_der > 1e-10f) ? 1.0f / max_abs_der : 0.0f;

    float y0 = combined_correlation_score(signal_value, signal_deriv, lag_m1, iv, id);
    float y1 = combined_correlation_score(signal_value, signal_deriv, best_lag, iv, id);
    float y2 = combined_correlation_score(signal_value, signal_deriv, lag_p1, iv, id);

    return static_cast<float>(best_lag) + sp::parabolic_peak_offset(y0, y1, y2);
}

// Trim value/derivative buffers symmetrically around the pass-1 symmetry axis,
// keeping the central `trim_fraction` of samples. Returns trim_start (offset
// into the original buffer where the trimmed region begins). Output buffers
// are populated with trim_start..trim_start+n_kept-1 from the inputs.
static size_t trim_around_symmetry_axis(
    const sfl::segmented_vector<float, 256> &signal_value,
    const sfl::segmented_vector<float, 256> &signal_deriv,
    int pass1_lag,
    float trim_fraction,
    sfl::segmented_vector<float, 256> &out_value,
    sfl::segmented_vector<float, 256> &out_deriv) {

    const size_t n = signal_value.size();
    // Symmetry axis (sample index) under pass-1 lag
    const float axis = (static_cast<float>(n) - 1.0f - static_cast<float>(pass1_lag)) * 0.5f;
    size_t n_kept = static_cast<size_t>(static_cast<float>(n) * trim_fraction + 0.5f);
    if (n_kept < 4) {
        n_kept = std::min(n, static_cast<size_t>(4));
    }
    if (n_kept > n) {
        n_kept = n;
    }
    // Centre n_kept samples on the axis (use ceil-style start to keep symmetry tight)
    int start_signed = static_cast<int>(axis - static_cast<float>(n_kept - 1) * 0.5f + 0.5f);
    if (start_signed < 0) {
        start_signed = 0;
    }
    if (start_signed + static_cast<int>(n_kept) > static_cast<int>(n)) {
        start_signed = static_cast<int>(n) - static_cast<int>(n_kept);
    }
    const size_t trim_start = static_cast<size_t>(start_signed);

    out_value.reserve(n_kept);
    for (size_t i = 0; i < n_kept; ++i) {
        out_value.push_back(signal_value[trim_start + i]);
    }
    // Derivative is one shorter than signal_value; keep matching range, clamped.
    const size_t deriv_n = signal_deriv.size();
    if (deriv_n > 0) {
        size_t d_end = std::min(trim_start + n_kept, deriv_n);
        size_t d_start = std::min(trim_start, d_end);
        out_deriv.reserve(d_end - d_start);
        for (size_t i = d_start; i < d_end; ++i) {
            out_deriv.push_back(signal_deriv[i]);
        }
    }
    return trim_start;
}

struct Pass2Result {
    int lag; // pass-2 best lag in original-signal coordinates
    float refined_lag; // parabolic-refined, original-signal coordinates
    float score; // combined correlation score on the trimmed window
    size_t n_kept;
    size_t trim_start;
};

// Pass-2 symmetry refinement: trim tails around the pass-1 axis and re-correlate
// over a small window. Returns nullopt when trim_fraction is outside (0, 1) or
// the trimmed window is too small to correlate.
static std::optional<Pass2Result> refine_with_trimmed_window(
    const sfl::segmented_vector<float, 256> &signal_value,
    const sfl::segmented_vector<float, 256> &signal_deriv,
    int pass1_lag,
    float dt,
    float trim_fraction) {

    if (!(trim_fraction > 0.0f && trim_fraction < 1.0f)) {
        return std::nullopt;
    }

    sfl::segmented_vector<float, 256> trimmed_value;
    sfl::segmented_vector<float, 256> trimmed_deriv;
    const size_t trim_start = trim_around_symmetry_axis(
        signal_value, signal_deriv, pass1_lag,
        trim_fraction, trimmed_value, trimmed_deriv);
    const size_t n_kept = trimmed_value.size();

    if (n_kept < 4 || trimmed_deriv.size() < 4) {
        return std::nullopt;
    }

    // In trimmed coordinates the symmetry axis is at the centre; expected lag ~0.
    // Search ±5 samples around 0.
    constexpr int search_radius = 5;
    const size_t trimmed_max_lag = std::min(n_kept / 2, static_cast<size_t>(0.5f / dt));
    const int t_lag_min = std::max(-search_radius, -static_cast<int>(trimmed_max_lag));
    const int t_lag_max = std::min(search_radius, static_cast<int>(trimmed_max_lag));

    auto t_value_corr = [&](int lag) { return sp::symmetry_correlation(trimmed_value, lag, 1.0f); };
    auto t_deriv_corr = [&](int lag) { return sp::symmetry_correlation(trimmed_deriv, lag, -1.0f); };

    int trimmed_best_lag = 0;
    float trimmed_best_combined = 0;
    find_best_correlation_lag(t_lag_min, t_lag_max, t_value_corr, t_deriv_corr,
        trimmed_best_lag, trimmed_best_combined);

    const float trimmed_refined = refine_lag_parabolic(trimmed_value, trimmed_deriv,
        trimmed_best_lag, trimmed_max_lag);

    // Convert trimmed lag back to original-signal lag coordinates.
    // For symmetric trim, the same physical axis corresponds to:
    //   original_lag = (n - 2*trim_start - n_kept) + trimmed_lag
    const size_t n = signal_value.size();
    const int lag_offset = static_cast<int>(n) - 2 * static_cast<int>(trim_start) - static_cast<int>(n_kept);

    return Pass2Result {
        .lag = lag_offset + trimmed_best_lag,
        .refined_lag = static_cast<float>(lag_offset) + trimmed_refined,
        .score = trimmed_best_combined,
        .n_kept = n_kept,
        .trim_start = trim_start,
    };
}

// Detect symmetry peak using value + derivative correlations.
static SymmetryPeakResult detect_symmetry_peak(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    size_t chunk_start_idx,
    size_t chunk_size,
    float chunk_start_time_s,
    float dt,
    float symmetry_trim_fraction,
    const char *label,
    int pass_num) {

    if (chunk_size < 10) {
        return { chunk_start_time_s + (chunk_size / 2) * dt, 0.0f, 0.0f, 0.0f };
    }

    sfl::segmented_vector<float, 256> signal_value;
    if (!preprocess_signal(raw_samples, chunk_start_idx, chunk_size, dt, signal_value)) {
        return { chunk_start_time_s + (chunk_size / 2) * dt, 0.0f, 0.0f, 0.0f };
    }

    debug_report_pass_raw_chunk(label, pass_num, chunk_start_idx, chunk_size, raw_samples);
    debug_report_pass_preprocessed(label, pass_num, chunk_start_idx, chunk_size, dt, signal_value);

    sfl::segmented_vector<float, 256> signal_deriv;
    compute_signal_derivative(signal_value, signal_deriv);

    debug_report_pass_derivative(label, pass_num, signal_deriv);

    size_t n = signal_value.size();
    size_t max_lag = std::min(n / 2, static_cast<size_t>(0.5f / dt));

    float best_combined = 0;
    int fine_min = 0, fine_max = 0;
    int best_lag = find_approx_symmetry_lag(signal_value, signal_deriv, max_lag, best_combined, fine_min, fine_max);

    debug_report_pass_correlation(label, pass_num, fine_min, fine_max, best_lag, signal_value, signal_deriv);

    const float refined_lag_full = refine_lag_parabolic(signal_value, signal_deriv, best_lag, max_lag);

    // Pass 2: trim tails symmetrically around the pass-1 symmetry axis and
    // re-correlate over a small window. This suppresses contributions from
    // saturated/flat tails that pull the peak.
    const auto pass2 = refine_with_trimmed_window(
        signal_value, signal_deriv, best_lag, dt, symmetry_trim_fraction);

    const float refined_lag = pass2 ? pass2->refined_lag : refined_lag_full;
    const float final_score = pass2 ? pass2->score : best_combined;

    debug_report_pass_trim_refine(label, pass_num,
        n, pass2 ? pass2->n_kept : 0, pass2 ? pass2->trim_start : 0,
        best_lag, best_combined, refined_lag_full,
        pass2 ? pass2->lag : best_lag,
        pass2 ? pass2->score : best_combined,
        pass2 ? pass2->refined_lag : refined_lag_full);

    // Convert refined lag to peak time
    float center_idx = (static_cast<float>(n - 1) - refined_lag) / 2.0f;
    center_idx = std::max(0.0f, std::min(static_cast<float>(n - 1), center_idx));
    float peak_time = chunk_start_time_s + center_idx * dt;

    float confidence = std::max(0.0f, std::min(1.0f, final_score / 2.0f));
    return { peak_time, confidence, final_score, best_combined };
}

// Round a duration in seconds to the nearest sample count at the given dt.
static constexpr int seconds_to_samples(float seconds, float dt) {
    return static_cast<int>(seconds / dt + 0.5f);
}

// Find the time shift τ that maximises the sum of energy at the four
// expected pass times — a 1-D matched filter against the motion profile.
static std::optional<int> find_rough_time_alignment(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float dt,
    const SweepSpeedProfile &profile,
    const char *label) {
    // As we do rough alignment, we can decimate it to make the computation faster...
    constexpr int decimation = 4;
    // ...and we don't need to search for large shifts:
    constexpr float max_shift_s = 0.5f;
    // The score `s_best / baseline` is bounded above by `n_dec / taps_total`
    // (the case where all energy falls inside the windows), so a fixed absolute
    // threshold cannot work across sweep lengths. Require the score to clear
    // this fraction of the gap between baseline (1.0) and that ceiling.
    constexpr float min_score_fraction = 0.5f;

    const size_t n = raw_samples.size();
    const float dt_dec = dt * decimation;
    const size_t n_dec = n / decimation;

    sfl::segmented_vector<float, 256> energy;
    {
        sp::MedianFilter<float, 5> med;
        size_t dec_count = 0;
        float acc = 0;
        for (size_t i = 0; i < n; ++i) {
            acc += med.filter(raw_samples[i].sensor_value);
            if (++dec_count == decimation) {
                energy.push_back(acc / decimation);
                acc = 0;
                dec_count = 0;
            }
        }
    }

    sp::linear_detrend(energy);

    float energy_sum = 0;
    for (size_t i = 0; i < n_dec; ++i) {
        energy[i] = energy[i] * energy[i];
        energy_sum += energy[i];
    }

    // Prepare the stencil of expected pass times, in decimated-sample units.
    // This is the set of indices we want to maximise energy at.
    const auto t_peaks = profile.expected_peak_times();
    std::array<int, std::tuple_size_v<decltype(t_peaks)>> offset_template;
    for (size_t i = 0; i < offset_template.size(); ++i) {
        offset_template[i] = seconds_to_samples(t_peaks[i], dt_dec);
    }

    // Pass-window half-widths in decimated samples — used both as the scoring
    // stencil and as the debug regions reported below. Integrating over the
    // full pass duration is much more robust than sampling four single
    // points, which is hypersensitive to small timing errors and noise spikes.
    const int hw1 = std::max(1, seconds_to_samples(profile.pass_time1() / 2.0f, dt_dec));
    const int hw2 = std::max(1, seconds_to_samples(profile.pass_time2() / 2.0f, dt_dec));

    // Sweep over candidate shifts, compute integrated energy in the four
    // pass windows for each shift, find the best.
    const int k_search = seconds_to_samples(max_shift_s, dt_dec);
    sfl::segmented_vector<float, 256> score_curve;
    score_curve.reserve(static_cast<size_t>(2 * k_search + 1));
    int k_best = 0;
    float s_best = -std::numeric_limits<float>::infinity();
    for (int k = -k_search; k <= k_search; ++k) {
        float s = 0;
        for (size_t t = 0; t < offset_template.size(); ++t) {
            const int hw = (t < 2) ? hw1 : hw2;
            const int center = offset_template[t] + k;
            const int j0 = std::max(0, center - hw);
            const int j1 = std::min(static_cast<int>(n_dec), center + hw);
            for (int j = j0; j < j1; ++j) {
                s += energy[j];
            }
        }
        score_curve.push_back(s);
        if (s > s_best) {
            s_best = s;
            k_best = k;
        }
    }

    // Check that the best score is sufficiently above the baseline (mean energy
    // per sample, scaled by the number of samples summed in the stencil:
    // 2·hw per window × 2 slow + 2 fast windows = 4·(hw1 + hw2)).
    const float taps_total = 4.0f * static_cast<float>(hw1 + hw2);
    const float baseline = energy_sum * (taps_total / static_cast<float>(n_dec));
    const float score_over_baseline = (baseline > 1e-10f) ? s_best / baseline : 0.0f;

    // Calculate coverage-adapted threshold: scales between 1.0 (uniform noise) and the
    // theoretical ceiling `n_dec / taps_total` (all energy concentrated in
    // windows). For short sweeps the windows cover little of the signal and
    // the ceiling is high; for long sweeps the ceiling drops toward 1.0.
    const float max_score = static_cast<float>(n_dec) / taps_total;
    const float min_score_over_baseline = 1.0f + min_score_fraction * (max_score - 1.0f);

    // Regions for debug plot — full pass duration around each tap.
    std::array<EnergyRegion, offset_template.size()> regions;
    for (size_t i = 0; i < offset_template.size(); ++i) {
        const int hw = (i < 2) ? hw1 : hw2;
        const int center = offset_template[i] + k_best;
        regions[i] = {
            static_cast<size_t>(std::clamp(center - hw, 0, static_cast<int>(n_dec))),
            static_cast<size_t>(std::clamp(center + hw, 0, static_cast<int>(n_dec))),
        };
    }

    debug_report_rough_align_score(label, dt_dec, -k_search, k_search, k_best,
        score_over_baseline, score_curve);
    debug_report_rough_align_energy(label, decimation, dt_dec, /*threshold=*/0.0f,
        /*offset=*/k_best * decimation,
        regions.data(), regions.size(), energy);

    // k_best at the edge of the search window means the true optimum likely
    // lies outside it; what we have is just a boundary clamp, not a real
    // alignment.
    if (std::abs(k_best) >= k_search - 1) {
        return std::nullopt;
    }

    if (score_over_baseline < min_score_over_baseline) {
        return std::nullopt;
    }
    return k_best * decimation;
}

// Detect peaks in 4 passes of two-speed sweep motion
static FourPassPeaks detect_four_peaks(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float dt,
    const SweepSpeedProfile &profile,
    uint32_t motion_profile_start_us,
    float symmetry_trim_fraction,
    const char *label) {
    constexpr uint8_t peaks_number = 4;
    FourPassPeaks result;
    const size_t n = raw_samples.size();

    auto t_peaks = profile.expected_peak_times();

    // Try data-driven rough alignment first
    auto rough_offset = find_rough_time_alignment(raw_samples, dt, profile, label);

    size_t pass_start_idx[peaks_number];
    size_t pass_end_idx[peaks_number];

    if (rough_offset.has_value()) {
        // Compute aligned peak centers in sample indices
        int aligned_peaks[peaks_number];
        for (int i = 0; i < peaks_number; ++i) {
            aligned_peaks[i] = static_cast<int>(t_peaks[i] / dt) + *rough_offset;
        }

        // Chunk boundaries: midpoints between adjacent peaks
        // First and last chunks are inset by preprocess_edge_margin so that
        // the preprocessing margin has real data to work with on both sides.
        pass_start_idx[0] = std::min(preprocess_edge_margin, n);
        pass_end_idx[0] = static_cast<size_t>((aligned_peaks[0] + aligned_peaks[1]) / 2);
        pass_end_idx[0] = std::min(pass_end_idx[0], n);
        pass_start_idx[1] = pass_end_idx[0];
        pass_end_idx[1] = static_cast<size_t>((aligned_peaks[1] + aligned_peaks[2]) / 2);
        pass_end_idx[1] = std::min(pass_end_idx[1], n);
        pass_start_idx[2] = pass_end_idx[1];
        pass_end_idx[2] = static_cast<size_t>((aligned_peaks[2] + aligned_peaks[3]) / 2);
        pass_end_idx[2] = std::min(pass_end_idx[2], n);
        pass_start_idx[3] = pass_end_idx[2];
        pass_end_idx[3] = (n > preprocess_edge_margin) ? n - preprocess_edge_margin : n;

        debug_report_rough_align("ok", *rough_offset, 0.0f, pass_start_idx, pass_end_idx);

        log_info(ContactlessOffset, "detect_four_peaks: using rough alignment, chunks: [%u-%u] [%u-%u] [%u-%u] [%u-%u]",
            static_cast<unsigned>(pass_start_idx[0]), static_cast<unsigned>(pass_end_idx[0]),
            static_cast<unsigned>(pass_start_idx[1]), static_cast<unsigned>(pass_end_idx[1]),
            static_cast<unsigned>(pass_start_idx[2]), static_cast<unsigned>(pass_end_idx[2]),
            static_cast<unsigned>(pass_start_idx[3]), static_cast<unsigned>(pass_end_idx[3]));
    } else {
        // Fallback: use profile timing with timestamp-based offset
        float sensor_offset_s = 0;
        if (!raw_samples.empty() && motion_profile_start_us != 0) {
            sensor_offset_s = static_cast<float>(
                                  static_cast<int32_t>(raw_samples[0].timestamp_us - motion_profile_start_us))
                / 1e6f;
        }

        log_info(ContactlessOffset, "detect_four_peaks: rough align failed, using profile timing (offset=%.3fms)",
            sensor_offset_s * 1000.0f);

        float pass_start_times[peaks_number];
        float pass_end_times[peaks_number];
        const float r = profile.rest_time;

        pass_start_times[0] = r;
        pass_end_times[0] = r + profile.pass_time1();
        pass_start_times[1] = pass_end_times[0] + r;
        pass_end_times[1] = pass_start_times[1] + profile.pass_time1();
        pass_start_times[2] = pass_end_times[1] + r;
        pass_end_times[2] = pass_start_times[2] + profile.pass_time2();
        pass_start_times[3] = pass_end_times[2] + r;
        pass_end_times[3] = pass_start_times[3] + profile.pass_time2();

        for (int i = 0; i < peaks_number; ++i) {
            float start_s = pass_start_times[i] - sensor_offset_s;
            float end_s = pass_end_times[i] - sensor_offset_s;
            pass_start_idx[i] = (start_s > 0) ? static_cast<size_t>(start_s / dt) : 0;
            pass_end_idx[i] = (end_s > 0) ? static_cast<size_t>(end_s / dt) : 0;
            pass_start_idx[i] = std::min(pass_start_idx[i], n);
            pass_end_idx[i] = std::min(pass_end_idx[i], n);
        }
        // Inset first/last chunks so preprocessing margin has data on both sides
        pass_start_idx[0] = std::max(pass_start_idx[0], preprocess_edge_margin);
        pass_end_idx[peaks_number - 1] = (pass_end_idx[peaks_number - 1] > preprocess_edge_margin)
            ? std::min(pass_end_idx[peaks_number - 1], n - preprocess_edge_margin)
            : pass_end_idx[peaks_number - 1];

        debug_report_rough_align("fallback", 0, sensor_offset_s * 1000.0f, pass_start_idx, pass_end_idx);
    }

    // Detect peak in each pass
    SymmetryPeakResult *pass_results[] = {
        &result.pass1, &result.pass2, &result.pass3, &result.pass4
    };

    uint32_t detect_four_start_us = ticks_us();

    for (int i = 0; i < peaks_number; ++i) {
        size_t chunk_start = pass_start_idx[i];
        size_t chunk_end = pass_end_idx[i];

        // chunk_start_time in sensor time (sample index × dt)
        float chunk_start_time_s = static_cast<float>(chunk_start) * dt;

        size_t chunk_size = 0;
        if (chunk_end > chunk_start) {
            chunk_size = chunk_end - chunk_start;
        }

        if (!chunk_size || chunk_start >= n) {
            *pass_results[i] = {
                chunk_start_time_s + static_cast<float>(chunk_size) * dt / 2.0f,
                0.0f,
                0.0f,
                0.0f
            };
            continue;
        }

        uint32_t pass_start_us = ticks_us();

        *pass_results[i] = detect_symmetry_peak(
            raw_samples,
            chunk_start,
            chunk_size,
            chunk_start_time_s,
            dt,
            symmetry_trim_fraction,
            label,
            i + 1);

        log_info(ContactlessOffset, "detect_four_peaks: pass %d took %uus (chunk_size=%u)",
            i + 1, static_cast<unsigned>(ticks_us() - pass_start_us), static_cast<unsigned>(chunk_size));
    }

    log_info(ContactlessOffset, "detect_four_peaks: total %uus", static_cast<unsigned>(ticks_us() - detect_four_start_us));

    return result;
}

// Estimate position by grid-searching the time offset that minimizes
// weighted peak position variance.
static PositionEstimate estimate_position_iterative(
    const float *peak_times,
    const float *weights,
    size_t n_peaks,
    const sfl::segmented_vector<float, 512> &position_table,
    sp::SamplingFreq motion_sampling_freq,
    float total_time,
    float max_delay_s) {

    uint32_t estimate_start_us = ticks_us();

    // Compute delay bounds from peak times: τ must map all peaks into [0, total_time]
    float min_peak = peak_times[0], max_peak = peak_times[0];
    for (size_t i = 1; i < n_peaks; ++i) {
        min_peak = std::min(min_peak, peak_times[i]);
        max_peak = std::max(max_peak, peak_times[i]);
    }
    float tau_lower = std::max(-max_delay_s, max_peak - total_time);
    float tau_upper = std::min(max_delay_s, min_peak);
    if (tau_upper <= tau_lower) {
        tau_lower = -max_delay_s;
        tau_upper = max_delay_s;
    }

    // Grid search parameters
    constexpr float delay_search_step_s = 0.0001f; // 0.1ms grid step
    const float delay_step = delay_search_step_s;
    int step_min = static_cast<int>(tau_lower / delay_step);
    int step_max = static_cast<int>(tau_upper / delay_step);

    float best_tau = 0;
    float min_variance = std::numeric_limits<float>::infinity();
    float best_mean_position = 0;

    const size_t table_size = position_table.size();
    if (table_size < 2) {
        return { 0, 0, std::numeric_limits<float>::infinity() };
    }

    // Reject peaks that land on flat regions (rest periods / turnarounds)
    // where position is not changing — these give degenerate zero-variance
    // solutions that are meaningless.
    constexpr float min_velocity_mm_s = 1.0f;

    // Grid search over time offset τ within computed bounds
    for (int step = step_min; step <= step_max; ++step) {
        float tau = static_cast<float>(step) * delay_step;

        float sum_wpos = 0;
        float sum_wpos_sq = 0;
        float sum_w = 0;
        size_t valid_count = 0;

        for (size_t i = 0; i < n_peaks; ++i) {
            float aligned_time = peak_times[i] - tau;

            if (aligned_time < 0 || aligned_time > total_time) {
                continue;
            }

            // O(1) lookup with linear interpolation for sub-step precision
            float fidx = aligned_time * motion_sampling_freq;
            size_t idx = static_cast<size_t>(fidx);
            if (idx >= table_size - 1) {
                idx = table_size - 2;
            }

            // Check local velocity — reject peaks on plateaus
            float velocity = std::abs(position_table[idx + 1] - position_table[idx]) * motion_sampling_freq;
            if (velocity < min_velocity_mm_s) {
                continue;
            }

            float frac = fidx - static_cast<float>(idx);
            float pos = position_table[idx] + frac * (position_table[idx + 1] - position_table[idx]);

            float w = weights ? weights[i] : 1.0f;
            sum_wpos += w * pos;
            sum_wpos_sq += w * pos * pos;
            sum_w += w;
            ++valid_count;
        }

        if (valid_count >= n_peaks && sum_w > 0) {
            float mean_pos = sum_wpos / sum_w;
            float variance = (sum_wpos_sq / sum_w) - (mean_pos * mean_pos);

            if (variance < min_variance) {
                min_variance = variance;
                best_tau = tau;
                best_mean_position = mean_pos;
            }
        }
    }

    log_info(ContactlessOffset, "estimate_position_iterative: n_peaks=%u n_steps=%d took %uus",
        static_cast<unsigned>(n_peaks), step_max - step_min + 1, static_cast<unsigned>(ticks_us() - estimate_start_us));

    return { best_mean_position, best_tau, min_variance };
}

// Compute final position estimate using all/forward/backward peak subsets
static TwoSpeedAnalysisResult estimate_position_from_peaks(
    const FourPassPeaks &peaks,
    const SweepSpeedProfile &profile,
    sp::SamplingFreq motion_sampling_freq) {

    constexpr float max_delay_fallback_s = 0.5f;
    constexpr float max_spread_mm = 0.1f; // spread above this → zero confidence

    uint32_t final_start_us = ticks_us();

    TwoSpeedAnalysisResult result;
    result.peaks = peaks;

    float all_peaks[4] = {
        peaks.pass1.peak_time_s,
        peaks.pass2.peak_time_s,
        peaks.pass3.peak_time_s,
        peaks.pass4.peak_time_s
    };
    float forward_peaks[2] = { peaks.pass1.peak_time_s, peaks.pass3.peak_time_s };
    float backward_peaks[2] = { peaks.pass2.peak_time_s, peaks.pass4.peak_time_s };

    const float dt = 1.0f / motion_sampling_freq;
    auto speed_source = profile.make_source(motion_sampling_freq);
    sfl::segmented_vector<float, 512> position_table;
    position_table.reserve(profile.total_samples(motion_sampling_freq));
    {
        float pos = 0;
        while (sp::pipe::available(speed_source)) {
            pos += speed_source.next() * dt;
            position_table.push_back(pos);
        }
    }

    log_info(ContactlessOffset, "estimate_position_from_peaks: built position_table with %u entries",
        static_cast<unsigned>(position_table.size()));

    // Dynamic max delay: at least 0.5s, or 10% of profile duration if larger
    float max_delay = std::max(max_delay_fallback_s, profile.total_time() * 0.1f);

    // Weight peaks by 1/speed: slow passes have better spatial resolution
    float w_slow = (profile.speed1 > 0) ? 1.0f / profile.speed1 : 1.0f;
    float w_fast = (profile.speed2 > 0) ? 1.0f / profile.speed2 : 1.0f;
    float all_weights[4] = { w_slow, w_slow, w_fast, w_fast };

    uint32_t est_all_start_us = ticks_us();
    result.estimate_all = estimate_position_iterative(
        all_peaks, all_weights, 4, position_table, motion_sampling_freq, profile.total_time(), max_delay);
    uint32_t est_all_us = ticks_us() - est_all_start_us;

    uint32_t est_fwd_start_us = ticks_us();
    result.estimate_forward = estimate_position_iterative(
        forward_peaks, nullptr, 2, position_table, motion_sampling_freq, profile.total_time(), max_delay);
    uint32_t est_fwd_us = ticks_us() - est_fwd_start_us;

    uint32_t est_bwd_start_us = ticks_us();
    result.estimate_backward = estimate_position_iterative(
        backward_peaks, nullptr, 2, position_table, motion_sampling_freq, profile.total_time(), max_delay);
    uint32_t est_bwd_us = ticks_us() - est_bwd_start_us;

    log_info(ContactlessOffset, "estimate_position_from_peaks: estimate_all=%uus estimate_fwd=%uus estimate_bwd=%uus",
        static_cast<unsigned>(est_all_us), static_cast<unsigned>(est_fwd_us), static_cast<unsigned>(est_bwd_us));

    result.delta_12_obs = peaks.pass2.peak_time_s - peaks.pass1.peak_time_s;
    result.delta_34_obs = peaks.pass4.peak_time_s - peaks.pass3.peak_time_s;
    result.delta_13_obs = peaks.pass3.peak_time_s - peaks.pass1.peak_time_s;
    result.delta_24_obs = peaks.pass4.peak_time_s - peaks.pass2.peak_time_s;

    // Model predictions: analytical deltas at estimated position
    float D = profile.total_distance;
    float p = result.estimate_all.position_mm;
    result.delta_12_model = (profile.speed1 > 0)
        ? profile.rest_time + 2.0f * (D - p) / profile.speed1
        : 0;
    result.delta_34_model = (profile.speed2 > 0)
        ? profile.rest_time + 2.0f * (D - p) / profile.speed2
        : 0;

    // Confidence from residual variance: spread of 0.1mm → 0 confidence
    float spread = std::sqrt(std::max(0.0f, result.estimate_all.residual_variance / 4.0f));
    result.confidence = std::max(0.0f, 1.0f - spread / max_spread_mm);

    log_info(ContactlessOffset, "estimate_position_from_peaks: total %uus", static_cast<unsigned>(ticks_us() - final_start_us));

    return result;
}

// Main entry point for two-speed sweep analysis
static std::expected<TwoSpeedAnalysisResult, const char *> analyze_twospeed_sweep(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float sensor_sampling_freq_hz,
    const SweepSpeedProfile &profile,
    sp::SamplingFreq motion_sampling_freq,
    uint32_t motion_profile_start_us,
    float symmetry_trim_fraction,
    const char *label) {

    constexpr size_t min_analysis_samples = 100;
    if (raw_samples.size() < min_analysis_samples) {
        return std::unexpected("Insufficient samples for analysis");
    }
    if (sensor_sampling_freq_hz <= 0) {
        return std::unexpected("Invalid sampling frequency");
    }

    float dt = 1.0f / sensor_sampling_freq_hz;

    uint32_t analysis_start_us = ticks_us();

    uint32_t peaks_start_us = ticks_us();
    FourPassPeaks peaks = detect_four_peaks(raw_samples, dt, profile, motion_profile_start_us, symmetry_trim_fraction, label);
    uint32_t peaks_us = ticks_us() - peaks_start_us;

    uint32_t final_result_start_us = ticks_us();
    TwoSpeedAnalysisResult result = estimate_position_from_peaks(peaks, profile, motion_sampling_freq);
    uint32_t final_result_us = ticks_us() - final_result_start_us;

    log_info(ContactlessOffset, "analyze_twospeed: total %uus (detect_peaks=%uus compute_result=%uus) samples=%u",
        static_cast<unsigned>(ticks_us() - analysis_start_us),
        static_cast<unsigned>(peaks_us),
        static_cast<unsigned>(final_result_us),
        static_cast<unsigned>(raw_samples.size()));

    return result;
}

static std::expected<TwoSpeedAnalysisResult, const char *> execute_and_analyze_sweep(
    const LineMotionConfig &config,
    tool_offset::Sensor &sensor,
    const char *label) {

    const float dx = config.end.x - config.start.x;
    const float dy = config.end.y - config.start.y;
    const float total_distance = std::sqrt(dx * dx + dy * dy);

    if (total_distance < 0.001f) {
        return std::unexpected("Line distance too small");
    }

    const float dir_x = dx / total_distance;
    const float dir_y = dy / total_distance;

    constexpr sp::SamplingFreq motion_sampling_freq = 1000.0f;

    auto profile = SweepSpeedProfile::from_config(config, total_distance);

    // Two separate sources needed: each is stateful and consumed independently by X/Y pipelines
    auto speed_source_x = profile.make_source(motion_sampling_freq);
    auto speed_source_y = profile.make_source(motion_sampling_freq);

    const size_t num_samples = profile.total_samples(motion_sampling_freq);

    auto motion_signal = create_motion_signal(
        speed_source_x, speed_source_y,
        dir_x, dir_y,
        num_samples, motion_sampling_freq);

    // Move to start position first
    do_blocking_move_to_xy(config.start);

    // Execute motion and record samples with extended wait for full profile duration
    uint32_t expected_duration_us = static_cast<uint32_t>(profile.total_time() * 1e6f);

    auto result_exp = execute_motion_with_recording(std::move(motion_signal), sensor, label, expected_duration_us);
    if (!result_exp.has_value()) {
        debug_report_analysis_error(label, result_exp.error());
        return std::unexpected(result_exp.error());
    }
    auto &result = *result_exp;

    uint32_t serial_output_start_us = ticks_us();
    debug_report_motion_profile(profile, motion_sampling_freq, label);
    debug_report_motion_timing(label, result);
    uint32_t serial_output_us = ticks_us() - serial_output_start_us;

    uint32_t analysis_start_us = ticks_us();
    auto analysis_result = analyze_twospeed_sweep(
        result.raw_samples,
        result.sensor_sampling_freq_hz,
        profile,
        motion_sampling_freq,
        result.motion_profile_start_us,
        config.symmetry_trim_fraction,
        label);
    uint32_t analysis_us = ticks_us() - analysis_start_us;

    log_info(ContactlessOffset, "execute_and_analyze_sweep: serial_output=%uus analysis=%uus",
        static_cast<unsigned>(serial_output_us), static_cast<unsigned>(analysis_us));

    if (!analysis_result.has_value()) {
        debug_report_analysis_error(label, analysis_result.error());
        return std::unexpected(analysis_result.error());
    }

    debug_report_twospeed_analysis(analysis_result.value(), profile, label);

    return analysis_result.value();
}

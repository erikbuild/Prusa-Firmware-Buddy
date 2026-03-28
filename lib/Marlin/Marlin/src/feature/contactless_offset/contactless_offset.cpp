#include "contactless_offset.hpp"

#include <gcode/gcode.h>
#include <module/motion.h>
#include <module/probe.h>
#include <module/temperature.h>
#include <module/planner.h>
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
#include <timing.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <sfl/segmented_vector.hpp>

LOG_COMPONENT_DEF(ContactlessOffset, logging::Severity::debug);

#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
static void serial_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    SerialUSB.cdc_write_sync(reinterpret_cast<uint8_t *>(buf), strlen(buf));
}
#endif

// Streaming reporter for raw sensor samples during a line scan
class LineSamplesDebugReporter {
#ifdef SERIAL_DEBUG
    const char *label;
    bool first_sample = true;
#endif

public:
    explicit LineSamplesDebugReporter([[maybe_unused]] const char *label)
#ifdef SERIAL_DEBUG
        : label(label)
#endif
    {
    }

    void start() {
#ifdef SERIAL_DEBUG
        serial_printf("# line_samples {\"label\": \"%s\", \"samples\": [", label);
#endif
    }

    void report_sample([[maybe_unused]] float sensor_value) {
#ifdef SERIAL_DEBUG
        if (!first_sample) {
            serial_printf(", ");
        } else {
            first_sample = false;
        }
        serial_printf("%.6f", sensor_value);
#endif
    }

    void finish([[maybe_unused]] float sampling_freq_hz) {
#ifdef SERIAL_DEBUG
        serial_printf("], \"sampling_freq_hz\": %.2f}\n", sampling_freq_hz);
#endif
    }
};

static void debug_report_probed_z([[maybe_unused]] float sensor_z, [[maybe_unused]] float offset) {
#ifdef SERIAL_DEBUG
    serial_printf("# probed_z {\"sensor_z\": %.6f, \"offset\": %.6f}\n", sensor_z, offset);
#endif
}

static void debug_report_scan_start([[maybe_unused]] const char *label) {
#ifdef SERIAL_DEBUG
    serial_printf("# scan_start {\"label\": \"%s\"}\n", label);
#endif
}

static void debug_report_scan_result([[maybe_unused]] const char *label, [[maybe_unused]] float confidence) {
#ifdef SERIAL_DEBUG
    serial_printf("# scan_result {\"label\": \"%s\", \"confidence\": %.3f}\n", label, confidence);
#endif
}

static void debug_report_pass1_center([[maybe_unused]] float x, [[maybe_unused]] float y) {
#ifdef SERIAL_DEBUG
    serial_printf("# pass1_center {\"x\": %.6f, \"y\": %.6f}\n", x, y);
#endif
}

static void debug_report_analysis_error([[maybe_unused]] const char *label, [[maybe_unused]] const char *error) {
#ifdef SERIAL_DEBUG
    serial_printf("# analysis_error {\"label\": \"%s\", \"error\": \"%s\"}\n", label, error);
#endif
}

static void debug_report_symmetry_search(
    [[maybe_unused]] unsigned n, [[maybe_unused]] unsigned max_lag,
    [[maybe_unused]] int coarse_lag, [[maybe_unused]] float coarse_score,
    [[maybe_unused]] int fine_min, [[maybe_unused]] int fine_max,
    [[maybe_unused]] int best_lag, [[maybe_unused]] float best_combined,
    [[maybe_unused]] float corr_at_0_val, [[maybe_unused]] float corr_at_0_der,
    [[maybe_unused]] float corr_at_best_val, [[maybe_unused]] float corr_at_best_der) {
#ifdef SERIAL_DEBUG
    serial_printf("# symmetry_debug {\"n\": %u, \"max_lag\": %u, "
                  "\"coarse_lag\": %d, \"coarse_score\": %.4f, "
                  "\"fine_range\": [%d, %d], \"best_lag\": %d, \"best_combined\": %.4f, "
                  "\"corr_at_0\": [%.4f, %.4f], \"corr_at_best\": [%.4f, %.4f]}\n",
        n, max_lag,
        coarse_lag, coarse_score,
        fine_min, fine_max, best_lag, best_combined,
        corr_at_0_val, corr_at_0_der, corr_at_best_val, corr_at_best_der);
#endif
}

static void debug_report_rough_align(
    [[maybe_unused]] const char *status,
    [[maybe_unused]] int offset,
    [[maybe_unused]] float extra_float,
    [[maybe_unused]] const size_t pass_start[4],
    [[maybe_unused]] const size_t pass_end[4]) {
#ifdef SERIAL_DEBUG
    serial_printf("# rough_align {\"status\": \"%s\", \"offset\": %d, \"extra\": %.3f, "
                  "\"chunks\": [[%u,%u],[%u,%u],[%u,%u],[%u,%u]]}\n",
        status, offset, extra_float,
        static_cast<unsigned>(pass_start[0]), static_cast<unsigned>(pass_end[0]),
        static_cast<unsigned>(pass_start[1]), static_cast<unsigned>(pass_end[1]),
        static_cast<unsigned>(pass_start[2]), static_cast<unsigned>(pass_end[2]),
        static_cast<unsigned>(pass_start[3]), static_cast<unsigned>(pass_end[3]));
#endif
}

template <typename Container>
static void debug_report_pass_preprocessed(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] int pass_num,
    [[maybe_unused]] size_t chunk_start,
    [[maybe_unused]] size_t chunk_size,
    [[maybe_unused]] float dt,
    [[maybe_unused]] const Container &signal_value) {
#ifdef SERIAL_DEBUG
    serial_printf("# pass_preprocessed {\"label\": \"%s\", \"pass\": %d, "
                  "\"chunk_start\": %u, \"chunk_size\": %u, \"dt\": %.6f, \"samples\": [",
        label, pass_num,
        static_cast<unsigned>(chunk_start),
        static_cast<unsigned>(chunk_size), dt);
    for (size_t i = 0; i < signal_value.size(); ++i) {
        if (i > 0) {
            serial_printf(", ");
        }
        serial_printf("%.6f", signal_value[i]);
    }
    serial_printf("]}\n");
#endif
}

template <typename Container>
static void debug_report_pass_derivative(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] int pass_num,
    [[maybe_unused]] const Container &signal_deriv) {
#ifdef SERIAL_DEBUG
    serial_printf("# pass_derivative {\"label\": \"%s\", \"pass\": %d, \"samples\": [",
        label, pass_num);
    for (size_t i = 0; i < signal_deriv.size(); ++i) {
        if (i > 0) {
            serial_printf(", ");
        }
        serial_printf("%.6f", signal_deriv[i]);
    }
    serial_printf("]}\n");
#endif
}

template <typename Container>
static void debug_report_pass_correlation(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] int pass_num,
    [[maybe_unused]] int fine_min,
    [[maybe_unused]] int fine_max,
    [[maybe_unused]] int best_lag,
    [[maybe_unused]] const Container &signal_value,
    [[maybe_unused]] const Container &signal_deriv) {
#ifdef SERIAL_DEBUG
    // Compute normalization factors over the fine range
    float max_abs_val = 0, max_abs_der = 0;
    for (int lag = fine_min; lag <= fine_max; ++lag) {
        max_abs_val = std::max(max_abs_val, std::abs(sp::symmetry_correlation(signal_value, lag, 1.0f)));
        max_abs_der = std::max(max_abs_der, std::abs(sp::symmetry_correlation(signal_deriv, lag, -1.0f)));
    }
    float inv_val = (max_abs_val > 1e-10f) ? 1.0f / max_abs_val : 0.0f;
    float inv_der = (max_abs_der > 1e-10f) ? 1.0f / max_abs_der : 0.0f;

    serial_printf("# pass_correlation {\"label\": \"%s\", \"pass\": %d, "
                  "\"fine_min\": %d, \"fine_max\": %d, \"best_lag\": %d, \"scores\": [",
        label, pass_num, fine_min, fine_max, best_lag);
    bool first = true;
    for (int lag = fine_min; lag <= fine_max; ++lag) {
        float cv = sp::symmetry_correlation(signal_value, lag, 1.0f) * inv_val;
        float cd = sp::symmetry_correlation(signal_deriv, lag, -1.0f) * inv_der;
        if (!first) {
            serial_printf(", ");
        }
        first = false;
        serial_printf("%.4f", cv + cd);
    }
    serial_printf("]}\n");
#endif
}

template <typename Container>
static void debug_report_rough_align_envelope(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] int decimation,
    [[maybe_unused]] float dt_dec,
    [[maybe_unused]] int best_offset,
    [[maybe_unused]] const int peak_offsets[4],
    [[maybe_unused]] const Container &envelope) {
#ifdef SERIAL_DEBUG
    serial_printf("# rough_align_envelope {\"label\": \"%s\", \"decimation\": %d, "
                  "\"dt_dec\": %.6f, \"best_offset\": %d, "
                  "\"peak_offsets\": [%d, %d, %d, %d], \"envelope\": [",
        label, decimation, dt_dec, best_offset,
        peak_offsets[0], peak_offsets[1], peak_offsets[2], peak_offsets[3]);
    for (size_t i = 0; i < envelope.size(); ++i) {
        if (i > 0) {
            serial_printf(", ");
        }
        serial_printf("%.4f", envelope[i]);
    }
    serial_printf("]}\n");
#endif
}

struct LineMotionConfig {
    xy_pos_t start;
    xy_pos_t end;
    float speed = 7.0f;
    float speed2 = 15.0f;
    float accel = 2000.0f;
    float rest_time = 0.0f;
};

struct SymmetryPeakResult {
    float peak_time_s;
    float confidence;
    float correlation_peak;
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

// Forward declaration — defined later in this file
static std::expected<TwoSpeedAnalysisResult, const char *> record_line_sweep(
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

    return probe_here(config.sensor_position.z);
}

std::expected<tool_offset::ToolOffset, const char *> tool_offset::measure_current_tool_offset(
    const tool_offset::ProbingConfig &config,
    tool_offset::Sensor &sensor) {

    // Check nozzle temperature before probing
    if (thermalManager.degHotend(0) > config.max_safe_temp) {
        return std::unexpected("Nozzle too hot for probing");
    }

    if (!GcodeSuite::G28_no_parser(true, true, true, G28Flags { .only_if_needed = true })) {
        return std::unexpected("Homing failed");
    }

    // Sensor may be below soft endstop limits — disable them for the whole measurement
    const bool saved_soft_endstops = soft_endstops_enabled;
    soft_endstops_enabled = false;
    ScopeGuard restore_soft_endstops([&] { soft_endstops_enabled = saved_soft_endstops; });

    const float safe_z = config.sensor_position.z + config.safe_z_height;
    do_blocking_move_to_z(safe_z);

    do_blocking_move_to_xy(config.sensor_position);

    const float sensor_z = measure_sensor_true_z(config);
    const float probing_z = sensor_z + config.sensing_z;
    do_blocking_move_to_z(probing_z);

    debug_report_probed_z(sensor_z, sensor_z - config.sensor_position.z);

    tool_offset::ToolOffset result;
    result.z = sensor_z - config.sensor_position.z;

    // XY offset measurement via two-pass scanning
    const float scan_half_width = config.sensing_diameter / 2.0f;
    const float sensor_x = config.sensor_position.x;
    const float sensor_y = config.sensor_position.y;

    // Helper: build a line scan config along one axis
    auto make_line_config = [&](float scan_start, float scan_end,
                                float cross_pos, bool along_x) {
        LineMotionConfig cfg;
        if (along_x) {
            cfg.start.set(scan_start, cross_pos);
            cfg.end.set(scan_end, cross_pos);
        } else {
            cfg.start.set(cross_pos, scan_start);
            cfg.end.set(cross_pos, scan_end);
        }
        cfg.speed = config.sensing_speed_slow;
        cfg.speed2 = config.sensing_speed_fast;
        cfg.rest_time = config.sweep_rest_time;
        return cfg;
    };

    // Helper: run one scan, return offset from scan center
    auto run_scan = [&](const char *name, bool along_x, float center, float cross_pos)
        -> std::expected<float, const char *> {
        auto cfg = make_line_config(center - scan_half_width, center + scan_half_width,
            cross_pos, along_x);
        debug_report_scan_start(name);
        auto scan_result = record_line_sweep(cfg, sensor, name);
        if (!scan_result.has_value()) {
            return std::unexpected(scan_result.error());
        }
        debug_report_scan_result(name, scan_result->confidence);
        return scan_result->estimate_all.position_mm - scan_half_width;
    };

    // Pass 1: center detection — scans centered on sensor_position
    auto cd_x = run_scan("center-detection-x", true, sensor_x, sensor_y);
    if (!cd_x.has_value()) {
        return std::unexpected(cd_x.error());
    }
    auto cd_y = run_scan("center-detection-y", false, sensor_y, sensor_x);
    if (!cd_y.has_value()) {
        return std::unexpected(cd_y.error());
    }

    const float max_offset = scan_half_width * 0.8f;
    const float cd_x_offset = std::clamp(*cd_x, -max_offset, max_offset);
    const float cd_y_offset = std::clamp(*cd_y, -max_offset, max_offset);

    debug_report_pass1_center(cd_x_offset, cd_y_offset);

    // Pass 2: nozzle offset — cross-axis corrected by pass-1 result
    auto no_x = run_scan("nozzle-offset-x", true, sensor_x, sensor_y + cd_y_offset);
    if (!no_x.has_value()) {
        return std::unexpected(no_x.error());
    }
    auto no_y = run_scan("nozzle-offset-y", false, sensor_y, sensor_x + cd_x_offset);
    if (!no_y.has_value()) {
        return std::unexpected(no_y.error());
    }

    result.x = *no_x;
    result.y = *no_y;

    return result;
}

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
        auto negate = [](float v) { return -v; };

        return sp::pipe::chain(
            rest(),
            trapezoid(speed1),
            rest(),
            sp::pipe::SignalSource<float>(trapezoid(speed1) | sp::pipe::transform(negate)),
            rest(),
            trapezoid(speed2),
            rest(),
            sp::pipe::SignalSource<float>(trapezoid(speed2) | sp::pipe::transform(negate)),
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
    uint32_t motion_profile_start_us; // time=0 of motion profile (for sensor alignment)

    uint32_t convert_time_ms;
    uint32_t steps_time_ms;
    uint32_t expected_time_ms;
    uint32_t actual_time_ms;
};

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
static MotionExecutionResult execute_motion_with_recording(
    MotionSignal &&motion_signal,
    tool_offset::Sensor &sensor,
    const char *label,
    uint32_t expected_duration_us) {

    MotionExecutionResult result = {};

    // Get mm per step from planner
    for (int ax = 0; ax < XYZE; ++ax) {
        if (planner.settings.axis_steps_per_mm[ax] <= 0) {
            result.raw_samples.clear();
            return result;
        }
    }
    abce_pos_t mm_per_step;
    mm_per_step.pos[0] = 1.0f / planner.settings.axis_steps_per_mm[X_AXIS];
    mm_per_step.pos[1] = 1.0f / planner.settings.axis_steps_per_mm[Y_AXIS];
    mm_per_step.pos[2] = 1.0f / planner.settings.axis_steps_per_mm[Z_AXIS];
    mm_per_step.pos[3] = 1.0f / planner.settings.axis_steps_per_mm[E_AXIS];

    // Debug reporter for samples
    LineSamplesDebugReporter samples_reporter(label);
    samples_reporter.start();

    sensor.start();
    if (!wait_for_first_sample(sensor)) {
        sensor.stop();
        return result;
    }

    // Enable steppers
    enable_all_steppers();

    auto receive_samples = [&]() {
        while (auto sample = sensor.get_sample()) {
            uint32_t now = ticks_us();
            result.raw_samples.push_back({ now, *sample });
            samples_reporter.report_sample(*sample);
        }
    };

    auto enqueue_step = [&, prev_ts = uint32_t { 0 }](uint32_t timestamp_us, AxisEnum axis, bool direction) mutable {
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
    sensor.stop();
    samples_reporter.finish(result.sensor_sampling_freq_hz);

    return result;
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

template <typename SpeedSource>
static void debug_report_motion_profile(
    [[maybe_unused]] SpeedSource &profile_source,
    [[maybe_unused]] sp::SamplingFreq sampling_freq,
    [[maybe_unused]] const char *label,
    [[maybe_unused]] const char *extra_json_fields) {

#ifdef SERIAL_DEBUG
    const float dt = 1.0f / sampling_freq;

    serial_printf("# motion_profile {\"label\": \"%s\"%s, \"header\": \"time_s, position_mm, velocity_mm_s\", \"samples\": [",
        label, extra_json_fields);
    bool first = true;
    float pos = 0.0f;
    float t = 0.0f;
    while (profile_source.available()) {
        float v = profile_source.next();
        if (!first) {
            serial_printf(", ");
        }
        first = false;
        serial_printf("[%.6f, %.6f, %.6f]", t, pos, v);
        pos += v * dt;
        t += dt;
    }
    serial_printf("]}\n");
#endif
}

static void debug_report_motion_timing(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] const MotionExecutionResult &result) {

#ifdef SERIAL_DEBUG
    float sensor_offset_ms = 0;
    if (!result.raw_samples.empty() && result.motion_profile_start_us != 0) {
        sensor_offset_ms = static_cast<float>(
                               static_cast<int32_t>(result.raw_samples[0].timestamp_us - result.motion_profile_start_us))
            / 1000.0f;
    }
    serial_printf("# motion_timing {\"label\": \"%s\", \"convert_ms\": %lu, \"steps_ms\": %lu, \"expected_ms\": %lu, \"actual_ms\": %lu, \"sensor_offset_ms\": %.3f}\n",
        label, result.convert_time_ms, result.steps_time_ms, result.expected_time_ms, result.actual_time_ms, sensor_offset_ms);
#endif
}

// Signal preprocessing: median filter + normalize + zero-phase lowpass + detrend
// Returns false if signal has no variance (flat).
static bool preprocess_signal(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    size_t chunk_start_idx,
    size_t chunk_size,
    float dt,
    sfl::segmented_vector<float, 256> &signal_value) {

    sp::MedianFilter<float, 5> median_filter;

    // Pass 1: accumulate stats through median filter for normalization.
    // Use double to avoid catastrophic cancellation: sensor values (~10^8 from
    // LDC1612) cause E[X^2]-E[X]^2 to lose all precision in float.
    double sum = 0, sum_sq = 0;
    for (size_t i = 0; i < chunk_size; ++i) {
        double v = static_cast<double>(median_filter.filter(raw_samples[chunk_start_idx + i].sensor_value));
        sum += v;
        sum_sq += v * v;
    }

    double d_mean = sum / static_cast<double>(chunk_size);
    double d_variance = (sum_sq / static_cast<double>(chunk_size)) - (d_mean * d_mean);
    float mean = static_cast<float>(d_mean);
    float std_val = static_cast<float>(std::sqrt(std::max(d_variance, 0.0)));

    if (std_val < 1e-10f) {
        return false;
    }

    // Pass 2: median + normalize
    constexpr float signal_lowpass_cutoff_hz = 50.0f;
    median_filter.reset();

    signal_value.reserve(chunk_size);
    for (size_t i = 0; i < chunk_size; ++i) {
        float median_filtered = median_filter.filter(raw_samples[chunk_start_idx + i].sensor_value);
        float normalized = (median_filtered - mean) / std_val;
        signal_value.push_back(normalized);
    }

    // Zero-phase lowpass with odd-extension edge padding.
    // Without padding, the biquad startup transient distorts the signal
    // near chunk edges, biasing the symmetry peak detection.
    {
        auto lowpass_coeffs = sp::butterworth_lowpass_biquad_2nd(signal_lowpass_cutoff_hz, 1.0f / dt);
        sp::Biquad<float> lp(lowpass_coeffs);

        const float sample_rate = 1.0f / dt;
        const size_t tau_samples = static_cast<size_t>(sample_rate / (2.0f * static_cast<float>(M_PI) * signal_lowpass_cutoff_hz));
        const size_t pad_len = std::min(10 * tau_samples, chunk_size - 1);

        sfl::segmented_vector<float, 256> padded;
        padded.reserve(chunk_size + 2 * pad_len);

        // Front: odd extension around signal_value[0]
        for (size_t i = pad_len; i >= 1; --i) {
            padded.push_back(2.0f * signal_value[0] - signal_value[i]);
        }
        for (size_t i = 0; i < chunk_size; ++i) {
            padded.push_back(signal_value[i]);
        }
        // Back: odd extension around signal_value[chunk_size-1]
        for (size_t i = 1; i <= pad_len; ++i) {
            padded.push_back(2.0f * signal_value[chunk_size - 1] - signal_value[chunk_size - 1 - i]);
        }

        const size_t pn = padded.size();
        for (size_t i = 0; i < pn; ++i) {
            padded[i] = lp.process(padded[i]);
        }
        lp.reset();
        for (size_t i = pn; i-- > 0;) {
            padded[i] = lp.process(padded[i]);
        }

        for (size_t i = 0; i < chunk_size; ++i) {
            signal_value[i] = padded[pad_len + i];
        }
    }

    sp::linear_detrend(signal_value);
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
// Computes sp::symmetry_correlation for both signals, normalizes each by
// its max absolute value over the 3-point neighborhood, then sums.
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
// Three sweeps: (1) value corr to find normalization, (2) deriv corr, (3) combined peak.
template <typename ValCorrFn, typename DerCorrFn>
static void search_correlation_range(
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
static int coarse_fine_symmetry_search(
    const sfl::segmented_vector<float, 256> &signal_value,
    const sfl::segmented_vector<float, 256> &signal_deriv,
    size_t max_lag,
    float &out_best_combined,
    int &out_fine_min,
    int &out_fine_max) {

    size_t n = signal_value.size();
    int best_lag = 0;
    out_best_combined = 0;

    auto value_corr = [&](int lag) { return sp::symmetry_correlation(signal_value, lag, 1.0f); };
    auto deriv_corr = [&](int lag) { return sp::symmetry_correlation(signal_deriv, lag, -1.0f); };

    constexpr size_t decimate_factor = 4;

    // Downsample with averaging for coarse search (avoids aliasing from sample-picking)
    size_t n_ds = n / decimate_factor;
    size_t nd_ds = signal_deriv.size() / decimate_factor;
    sfl::segmented_vector<float, 256> val_ds, der_ds;
    val_ds.reserve(n_ds);
    der_ds.reserve(nd_ds);
    for (size_t i = 0; i < n_ds; ++i) {
        float sum_v = 0;
        for (size_t j = 0; j < decimate_factor; ++j) {
            sum_v += signal_value[i * decimate_factor + j];
        }
        val_ds.push_back(sum_v / static_cast<float>(decimate_factor));
    }
    for (size_t i = 0; i < nd_ds; ++i) {
        float sum_d = 0;
        for (size_t j = 0; j < decimate_factor; ++j) {
            sum_d += signal_deriv[i * decimate_factor + j];
        }
        der_ds.push_back(sum_d / static_cast<float>(decimate_factor));
    }

    auto ds_val_corr = [&](int lag) { return sp::symmetry_correlation(val_ds, lag, 1.0f); };
    auto ds_der_corr = [&](int lag) { return sp::symmetry_correlation(der_ds, lag, -1.0f); };

    int max_lag_ds = static_cast<int>(max_lag / decimate_factor);
    int coarse_best = 0;
    float coarse_combined = 0;
    search_correlation_range(-max_lag_ds, max_lag_ds, ds_val_corr, ds_der_corr, coarse_best, coarse_combined);

    // Fine pass around coarse result — generous radius since correlation eval is cheap
    int fine_center = coarse_best * static_cast<int>(decimate_factor);
    int fine_radius = static_cast<int>(8 * decimate_factor);
    int fine_min = std::max(fine_center - fine_radius, -static_cast<int>(max_lag));
    int fine_max = std::min(fine_center + fine_radius, static_cast<int>(max_lag));
    out_fine_min = fine_min;
    out_fine_max = fine_max;

    search_correlation_range(fine_min, fine_max, value_corr, deriv_corr, best_lag, out_best_combined);

    // Correlation at lag=0 (perfect symmetry) and at a few key points for diagnostics
    float corr_at_0_val = sp::symmetry_correlation(signal_value, 0, 1.0f);
    float corr_at_0_der = sp::symmetry_correlation(signal_deriv, 0, -1.0f);
    float corr_at_best_val = sp::symmetry_correlation(signal_value, best_lag, 1.0f);
    float corr_at_best_der = sp::symmetry_correlation(signal_deriv, best_lag, -1.0f);

    debug_report_symmetry_search(
        static_cast<unsigned>(n), static_cast<unsigned>(max_lag),
        coarse_best, coarse_combined,
        fine_min, fine_max, best_lag, out_best_combined,
        corr_at_0_val, corr_at_0_der, corr_at_best_val, corr_at_best_der);

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

// Detect symmetry peak using value + derivative correlations.
// Median-filters and lowpass-filters the signal, then finds the lag where
// the signal best matches its own reverse (symmetry axis = peak center).
static SymmetryPeakResult detect_symmetry_peak_streaming(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    size_t chunk_start_idx,
    size_t chunk_size,
    float chunk_start_time_s,
    float dt,
    const char *label,
    int pass_num) {

    if (chunk_size < 10) {
        return { chunk_start_time_s + (chunk_size / 2) * dt, 0.0f, 0.0f };
    }

    // Preprocess: median + normalize + zero-phase lowpass + detrend
    sfl::segmented_vector<float, 256> signal_value;
    if (!preprocess_signal(raw_samples, chunk_start_idx, chunk_size, dt, signal_value)) {
        return { chunk_start_time_s + (chunk_size / 2) * dt, 0.0f, 0.0f };
    }

    debug_report_pass_preprocessed(label, pass_num, chunk_start_idx, chunk_size, dt, signal_value);

    // Derivative with unit-variance normalization
    sfl::segmented_vector<float, 256> signal_deriv;
    compute_signal_derivative(signal_value, signal_deriv);

    debug_report_pass_derivative(label, pass_num, signal_deriv);

    // Coarse-to-fine correlation search
    size_t n = signal_value.size();
    size_t max_lag = std::min(n / 2, static_cast<size_t>(0.5f / dt));

    float best_combined = 0;
    int fine_min = 0, fine_max = 0;
    int best_lag = coarse_fine_symmetry_search(signal_value, signal_deriv, max_lag, best_combined, fine_min, fine_max);

    debug_report_pass_correlation(label, pass_num, fine_min, fine_max, best_lag, signal_value, signal_deriv);

    // Sub-sample refinement
    float refined_lag = refine_lag_parabolic(signal_value, signal_deriv, best_lag, max_lag);

    // Convert refined lag to peak time
    float center_idx = (static_cast<float>(n - 1) - refined_lag) / 2.0f;
    center_idx = std::max(0.0f, std::min(static_cast<float>(n - 1), center_idx));
    float peak_time = chunk_start_time_s + center_idx * dt;

    float confidence = std::max(0.0f, std::min(1.0f, best_combined / 2.0f));
    return { peak_time, confidence, best_combined };
}

// Rough alignment: find the sample offset that best aligns the expected
// peak spacing pattern with the actual signal. Uses zero-phase lowpass
// filtering to merge double peaks, then slides the expected peak template
// across the envelope to maximize the sum of envelope values at peak positions.
// Returns the best offset in samples, or nullopt on failure.
static std::optional<int> rough_align_offset(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float dt,
    const SweepSpeedProfile &profile,
    const char *label) {

    constexpr size_t min_rough_align_samples = 20;
    constexpr float envelope_lowpass_cutoff_hz = 4.0f;
    constexpr int decimation = 4;

    const size_t n = raw_samples.size();
    if (n < min_rough_align_samples) {
        return std::nullopt;
    }

    // Median-filter and decimate the raw samples
    const float dt_dec = dt * decimation;
    const size_t n_dec = n / decimation;
    sfl::segmented_vector<float, 256> envelope;
    {
        sp::MedianFilter<float, 5> med;
        size_t dec_count = 0;
        float acc = 0;
        for (size_t i = 0; i < n; ++i) {
            acc += med.filter(raw_samples[i].sensor_value);
            if (++dec_count == decimation) {
                envelope.push_back(acc / decimation);
                acc = 0;
                dec_count = 0;
            }
        }
    }

    // Zero-phase lowpass with odd-extension edge padding.
    // Without padding, the biquad startup transient corrupts envelope edges,
    // biasing the template match by 100+ samples.
    {
        auto lp_coeffs = sp::butterworth_lowpass_biquad_2nd(envelope_lowpass_cutoff_hz, 1.0f / dt_dec);
        sp::Biquad<float> lp(lp_coeffs);

        const float sample_rate_dec = 1.0f / dt_dec;
        const size_t tau_samples = static_cast<size_t>(sample_rate_dec / (2.0f * static_cast<float>(M_PI) * envelope_lowpass_cutoff_hz));
        const size_t pad_len = std::min(10 * tau_samples, n_dec - 1);

        sfl::segmented_vector<float, 256> padded;

        // Front: odd extension around envelope[0]
        for (size_t i = pad_len; i >= 1; --i) {
            padded.push_back(2.0f * envelope[0] - envelope[i]);
        }
        for (size_t i = 0; i < n_dec; ++i) {
            padded.push_back(envelope[i]);
        }
        // Back: odd extension around envelope[n_dec-1]
        for (size_t i = 1; i <= pad_len; ++i) {
            padded.push_back(2.0f * envelope[n_dec - 1] - envelope[n_dec - 1 - i]);
        }

        const size_t pn = padded.size();
        for (size_t i = 0; i < pn; ++i) {
            padded[i] = lp.process(padded[i]);
        }
        lp.reset();
        for (size_t i = pn; i-- > 0;) {
            padded[i] = lp.process(padded[i]);
        }

        for (size_t i = 0; i < n_dec; ++i) {
            envelope[i] = padded[pad_len + i];
        }
    }

    sp::linear_detrend(envelope);

    // Template matching in decimated domain
    auto t_peaks = profile.expected_peak_times();
    int peak_offsets[4];
    for (int i = 0; i < 4; ++i) {
        peak_offsets[i] = static_cast<int>(t_peaks[i] / dt_dec);
    }
    int template_span = peak_offsets[3];

    int search_min = -peak_offsets[0];
    int search_max = static_cast<int>(n_dec) - template_span - 1;

    if (search_max <= search_min) {
        return std::nullopt;
    }

    float best_score = -std::numeric_limits<float>::infinity();
    int best_offset = 0;

    for (int tau = search_min; tau <= search_max; ++tau) {
        float score = 0;
        bool valid = true;
        for (int i = 0; i < 4; ++i) {
            int idx = peak_offsets[i] + tau;
            if (idx < 0 || idx >= static_cast<int>(n_dec)) {
                valid = false;
                break;
            }
            score += envelope[idx];
        }
        if (valid && score > best_score) {
            best_score = score;
            best_offset = tau;
        }
    }

    debug_report_rough_align_envelope(label, decimation, dt_dec, best_offset, peak_offsets, envelope);

    // Convert back to original sample domain
    int offset_original = best_offset * decimation;

    log_info(ContactlessOffset, "rough_align: best_offset=%d (%.1fms) score=%.3f search=[%d, %d] (dec=%d)",
        offset_original, offset_original * dt * 1000.0f, best_score,
        search_min, search_max, decimation);

    return offset_original;
}

// Detect peaks in 4 passes of two-speed sweep motion
static FourPassPeaks detect_four_peaks(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float dt,
    const SweepSpeedProfile &profile,
    uint32_t motion_profile_start_us,
    const char *label) {

    FourPassPeaks result;
    const size_t n = raw_samples.size();

    auto t_peaks = profile.expected_peak_times();

    // Try data-driven rough alignment first
    auto rough_offset = rough_align_offset(raw_samples, dt, profile, label);

    size_t pass_start_idx[4];
    size_t pass_end_idx[4];

    if (rough_offset.has_value()) {
        // Compute aligned peak centers in sample indices
        int aligned_peaks[4];
        for (int i = 0; i < 4; ++i) {
            aligned_peaks[i] = static_cast<int>(t_peaks[i] / dt) + *rough_offset;
        }

        // Chunk boundaries: midpoints between adjacent peaks
        // First chunk starts at 0, last chunk ends at n
        pass_start_idx[0] = 0;
        pass_end_idx[0] = static_cast<size_t>((aligned_peaks[0] + aligned_peaks[1]) / 2);
        pass_start_idx[1] = pass_end_idx[0];
        pass_end_idx[1] = static_cast<size_t>((aligned_peaks[1] + aligned_peaks[2]) / 2);
        pass_start_idx[2] = pass_end_idx[1];
        pass_end_idx[2] = static_cast<size_t>((aligned_peaks[2] + aligned_peaks[3]) / 2);
        pass_start_idx[3] = pass_end_idx[2];
        pass_end_idx[3] = n;

        // Clamp all to valid range
        for (int i = 0; i < 4; ++i) {
            pass_start_idx[i] = std::min(pass_start_idx[i], n);
            pass_end_idx[i] = std::min(pass_end_idx[i], n);
        }

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

        float pass_start_times[4];
        float pass_end_times[4];
        const float r = profile.rest_time;

        pass_start_times[0] = r;
        pass_end_times[0] = r + profile.pass_time1();
        pass_start_times[1] = pass_end_times[0] + r;
        pass_end_times[1] = pass_start_times[1] + profile.pass_time1();
        pass_start_times[2] = pass_end_times[1] + r;
        pass_end_times[2] = pass_start_times[2] + profile.pass_time2();
        pass_start_times[3] = pass_end_times[2] + r;
        pass_end_times[3] = pass_start_times[3] + profile.pass_time2();

        for (int i = 0; i < 4; ++i) {
            float start_s = pass_start_times[i] - sensor_offset_s;
            float end_s = pass_end_times[i] - sensor_offset_s;
            pass_start_idx[i] = (start_s > 0) ? static_cast<size_t>(start_s / dt) : 0;
            pass_end_idx[i] = (end_s > 0) ? static_cast<size_t>(end_s / dt) : 0;
            pass_start_idx[i] = std::min(pass_start_idx[i], n);
            pass_end_idx[i] = std::min(pass_end_idx[i], n);
        }

        debug_report_rough_align("fallback", 0, sensor_offset_s * 1000.0f, pass_start_idx, pass_end_idx);
    }

    // Detect peak in each pass
    SymmetryPeakResult *pass_results[] = {
        &result.pass1, &result.pass2, &result.pass3, &result.pass4
    };

    uint32_t detect_four_start_us = ticks_us();

    for (int i = 0; i < 4; ++i) {
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
                0.0f
            };
            continue;
        }

        uint32_t pass_start_us = ticks_us();

        *pass_results[i] = detect_symmetry_peak_streaming(
            raw_samples,
            chunk_start,
            chunk_size,
            chunk_start_time_s,
            dt,
            label,
            i + 1);

        log_info(ContactlessOffset, "detect_four_peaks: pass %d took %uus (chunk_size=%u)",
            i + 1, static_cast<unsigned>(ticks_us() - pass_start_us), static_cast<unsigned>(chunk_size));
    }

    log_info(ContactlessOffset, "detect_four_peaks: total %uus", static_cast<unsigned>(ticks_us() - detect_four_start_us));

    return result;
}

// Estimate position by grid-searching the time offset that minimizes
// weighted peak position variance. When weights is null, uniform weighting.
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
static TwoSpeedAnalysisResult compute_final_result(
    const FourPassPeaks &peaks,
    const SweepSpeedProfile &profile,
    sp::SamplingFreq motion_sampling_freq) {

    constexpr float max_delay_fallback_s = 0.5f;
    constexpr float max_spread_mm = 1.0f; // spread above this → zero confidence

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

    // Position lookup table: O(1) per query vs. integrating each time
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

    log_info(ContactlessOffset, "compute_final_result: built position_table with %u entries",
        static_cast<unsigned>(position_table.size()));

    // Dynamic max delay: min(0.5s, 10% of profile duration)
    float max_delay = std::min(max_delay_fallback_s, profile.total_time() * 0.1f);

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

    log_info(ContactlessOffset, "compute_final_result: estimate_all=%uus estimate_fwd=%uus estimate_bwd=%uus",
        static_cast<unsigned>(est_all_us), static_cast<unsigned>(est_fwd_us), static_cast<unsigned>(est_bwd_us));

    result.delta_12_obs = peaks.pass2.peak_time_s - peaks.pass1.peak_time_s;
    result.delta_34_obs = peaks.pass4.peak_time_s - peaks.pass3.peak_time_s;
    result.delta_13_obs = peaks.pass3.peak_time_s - peaks.pass1.peak_time_s;
    result.delta_24_obs = peaks.pass4.peak_time_s - peaks.pass2.peak_time_s;

    // Model predictions: analytical deltas at estimated position
    // Δ12 = rest + 2*(D-p)/v1,  Δ34 = rest + 2*(D-p)/v2
    float D = profile.total_distance;
    float p = result.estimate_all.position_mm;
    result.delta_12_model = (profile.speed1 > 0)
        ? profile.rest_time + 2.0f * (D - p) / profile.speed1
        : 0;
    result.delta_34_model = (profile.speed2 > 0)
        ? profile.rest_time + 2.0f * (D - p) / profile.speed2
        : 0;

    // Confidence from residual variance: spread of 1mm → 0 confidence
    float spread = std::sqrt(std::max(0.0f, result.estimate_all.residual_variance / 4.0f));
    result.confidence = std::max(0.0f, 1.0f - spread / max_spread_mm);

    log_info(ContactlessOffset, "compute_final_result: total %uus", static_cast<unsigned>(ticks_us() - final_start_us));

    return result;
}

static void debug_report_twospeed_analysis(
    [[maybe_unused]] const TwoSpeedAnalysisResult &result,
    [[maybe_unused]] const SweepSpeedProfile &profile,
    [[maybe_unused]] const char *label) {

#ifdef SERIAL_DEBUG
    char name[64];
    snprintf(name, sizeof(name), "%.0f/%.0f mm/s, %.0fmm",
        profile.speed1, profile.speed2, profile.total_distance);

    serial_printf("# twospeed_peaks {\"label\": \"%s\", \"name\": \"%s\", "
                  "\"t1\": %.6f, \"t2\": %.6f, \"t3\": %.6f, \"t4\": %.6f, "
                  "\"confidence\": %.3f}\n",
        label, name,
        result.peaks.pass1.peak_time_s,
        result.peaks.pass2.peak_time_s,
        result.peaks.pass3.peak_time_s,
        result.peaks.pass4.peak_time_s,
        result.confidence);

    serial_printf("# twospeed_estimates {\"label\": \"%s\", \"name\": \"%s\", "
                  "\"all\": {\"pos\": %.6f, \"tau\": %.6f, \"residual\": %.6f}, "
                  "\"forward\": {\"pos\": %.6f, \"tau\": %.6f, \"residual\": %.6f}, "
                  "\"backward\": {\"pos\": %.6f, \"tau\": %.6f, \"residual\": %.6f}}\n",
        label, name,
        result.estimate_all.position_mm,
        result.estimate_all.time_offset_s,
        result.estimate_all.residual_variance,
        result.estimate_forward.position_mm,
        result.estimate_forward.time_offset_s,
        result.estimate_forward.residual_variance,
        result.estimate_backward.position_mm,
        result.estimate_backward.time_offset_s,
        result.estimate_backward.residual_variance);

    serial_printf("# twospeed_deltas {\"label\": \"%s\", \"name\": \"%s\", "
                  "\"delta_12_obs\": %.6f, \"delta_34_obs\": %.6f, "
                  "\"delta_12_model\": %.6f, \"delta_34_model\": %.6f, "
                  "\"delta_13_obs\": %.6f, \"delta_24_obs\": %.6f}\n",
        label, name,
        result.delta_12_obs, result.delta_34_obs,
        result.delta_12_model, result.delta_34_model,
        result.delta_13_obs, result.delta_24_obs);

    const SymmetryPeakResult *passes[] = {
        &result.peaks.pass1, &result.peaks.pass2,
        &result.peaks.pass3, &result.peaks.pass4
    };
    for (int i = 0; i < 4; ++i) {
        const auto &pass = *passes[i];
        serial_printf("# twospeed_symmetry {\"label\": \"%s\", \"name\": \"%s\", "
                      "\"pass\": %d, "
                      "\"peak_time\": %.6f, \"confidence\": %.3f, "
                      "\"correlation_peak\": %.3f}\n",
            label, name, i + 1,
            pass.peak_time_s,
            pass.confidence,
            pass.correlation_peak);
    }
#endif
}

// Main entry point for two-speed sweep analysis
static std::expected<TwoSpeedAnalysisResult, const char *> analyze_twospeed_sweep(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float sensor_sampling_freq_hz,
    const SweepSpeedProfile &profile,
    sp::SamplingFreq motion_sampling_freq,
    uint32_t motion_profile_start_us,
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
    FourPassPeaks peaks = detect_four_peaks(raw_samples, dt, profile, motion_profile_start_us, label);
    uint32_t peaks_us = ticks_us() - peaks_start_us;

    uint32_t final_result_start_us = ticks_us();
    TwoSpeedAnalysisResult result = compute_final_result(peaks, profile, motion_sampling_freq);
    uint32_t final_result_us = ticks_us() - final_result_start_us;

    log_info(ContactlessOffset, "analyze_twospeed: total %uus (detect_peaks=%uus compute_result=%uus) samples=%u",
        static_cast<unsigned>(ticks_us() - analysis_start_us),
        static_cast<unsigned>(peaks_us),
        static_cast<unsigned>(final_result_us),
        static_cast<unsigned>(raw_samples.size()));

    return result;
}

static std::expected<TwoSpeedAnalysisResult, const char *> record_line_sweep(
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
    auto result = execute_motion_with_recording(
        std::move(motion_signal), sensor, label, expected_duration_us);

    uint32_t serial_output_start_us = ticks_us();
    auto profile_source = profile.make_source(motion_sampling_freq);
    char extra_fields[128];
    snprintf(extra_fields, sizeof(extra_fields),
        ", \"mode\": \"twospeed\", \"speed1\": %.2f, \"speed2\": %.2f, \"rest_time\": %.6f",
        profile.speed1, profile.speed2, profile.rest_time);
    debug_report_motion_profile(profile_source, motion_sampling_freq, label, extra_fields);

    debug_report_motion_timing(label, result);
    uint32_t serial_output_us = ticks_us() - serial_output_start_us;

    uint32_t analysis_start_us = ticks_us();
    auto analysis_result = analyze_twospeed_sweep(
        result.raw_samples,
        result.sensor_sampling_freq_hz,
        profile,
        motion_sampling_freq,
        result.motion_profile_start_us,
        label);
    uint32_t analysis_us = ticks_us() - analysis_start_us;

    log_info(ContactlessOffset, "record_line_sweep: serial_output=%uus analysis=%uus",
        static_cast<unsigned>(serial_output_us), static_cast<unsigned>(analysis_us));

    if (!analysis_result.has_value()) {
        debug_report_analysis_error(label, analysis_result.error());
        return std::unexpected(analysis_result.error());
    }

    debug_report_twospeed_analysis(analysis_result.value(), profile, label);

    return analysis_result.value();
}

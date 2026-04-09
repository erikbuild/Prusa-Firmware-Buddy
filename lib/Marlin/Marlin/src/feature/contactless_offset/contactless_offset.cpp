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
// Each axis (X, Y) is measured independently. We do it in two rounds:
// first a rough "center detection" scan centered on the nominal sensor
// position, then a refined "nozzle offset" scan where the cross-axis
// position is corrected by the first round's result, so the nozzle passes
// through the strongest part of the sensor field.
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
#include <timing.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <sfl/segmented_vector.hpp>

LOG_COMPONENT_DEF(ContactlessOffset, logging::Severity::debug);

#define SERIAL_DEBUG

// Margin (in samples) added around each chunk to suppress filter edge artifacts.
static constexpr size_t preprocess_edge_margin = 10;

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
#include "debug_reporters.defs"

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

    LineSamplesDebugReporter samples_reporter(label);
    samples_reporter.start();

    sensor.start();
    if (!wait_for_first_sample(sensor)) {
        sensor.stop();
        return result;
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

    auto run_scan = [&](const char *name, bool along_x, float center, float cross_pos)
        -> std::expected<float, const char *> {
        auto cfg = make_line_config(center - scan_half_width, center + scan_half_width,
            cross_pos, along_x);
        debug_report_scan_start(name);
        auto scan_result = execute_and_analyze_sweep(cfg, sensor, name);
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

// Detect symmetry peak using value + derivative correlations.
static SymmetryPeakResult detect_symmetry_peak(
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

    sfl::segmented_vector<float, 256> signal_value;
    if (!preprocess_signal(raw_samples, chunk_start_idx, chunk_size, dt, signal_value)) {
        return { chunk_start_time_s + (chunk_size / 2) * dt, 0.0f, 0.0f };
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

    float refined_lag = refine_lag_parabolic(signal_value, signal_deriv, best_lag, max_lag);

    // Convert refined lag to peak time
    float center_idx = (static_cast<float>(n - 1) - refined_lag) / 2.0f;
    center_idx = std::max(0.0f, std::min(static_cast<float>(n - 1), center_idx));
    float peak_time = chunk_start_time_s + center_idx * dt;

    float confidence = std::max(0.0f, std::min(1.0f, best_combined / 2.0f));
    return { peak_time, confidence, best_combined };
}

// Rough alignment: find the sample offset that best aligns the expected
// peak spacing pattern with the actual signal.
static std::optional<int> find_rough_time_alignment(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float dt,
    const SweepSpeedProfile &profile,
    const char *label) {

    constexpr size_t min_rough_align_samples = 20;
    constexpr int decimation = 4;
    constexpr float energy_lowpass_cutoff_hz = 4.0f;
    constexpr float threshold_fraction = 0.08f;
    constexpr float hysteresis_ratio = 0.5f;
    constexpr float min_gap_s = 0.05f;
    constexpr float min_region_s = 0.1f;
    constexpr float duration_tolerance = 0.5f;

    const size_t n = raw_samples.size();
    if (n < min_rough_align_samples) {
        return std::nullopt;
    }

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

    // Square, lowpass, and find peak in a single pass
    {
        auto lp_coeffs = sp::butterworth_lowpass_biquad_1st(energy_lowpass_cutoff_hz, 1.0f / dt_dec);
        sp::Biquad<float> lp(lp_coeffs);
        for (size_t i = 0; i < n_dec; ++i) {
            energy[i] = lp.process(energy[i] * energy[i]);
        }
    }

    float max_energy = *std::max_element(energy.begin(), energy.end());
    if (max_energy < 1e-10f) {
        return std::nullopt;
    }

    const float threshold_high = threshold_fraction * max_energy;
    const float threshold_low = threshold_high * hysteresis_ratio;

    constexpr size_t max_regions = 8;
    EnergyRegion regions[max_regions];
    size_t num_regions = 0;
    bool in_active = false;
    size_t region_start = 0;

    for (size_t i = 0; i < n_dec; ++i) {
        if (!in_active) {
            if (energy[i] >= threshold_high) {
                in_active = true;
                region_start = i;
            }
        } else {
            if (energy[i] < threshold_low) {
                in_active = false;
                if (num_regions < max_regions) {
                    regions[num_regions++] = { region_start, i };
                }
            }
        }
    }
    if (in_active && num_regions < max_regions) {
        regions[num_regions++] = { region_start, n_dec };
    }

    // Merge regions with small gaps (handles W-shape dip splitting), then discard tiny ones
    const size_t min_gap_samples = static_cast<size_t>(min_gap_s / dt_dec);
    const size_t min_region_samples = static_cast<size_t>(min_region_s / dt_dec);
    {
        size_t write = 0;
        for (size_t r = 0; r < num_regions; ++r) {
            if (write > 0 && (regions[r].start - regions[write - 1].end) < min_gap_samples) {
                regions[write - 1].end = regions[r].end;
            } else {
                regions[write++] = regions[r];
            }
        }
        num_regions = write;

        write = 0;
        for (size_t r = 0; r < num_regions; ++r) {
            if ((regions[r].end - regions[r].start) >= min_region_samples) {
                regions[write++] = regions[r];
            }
        }
        num_regions = write;
    }

    auto fail = [&](int offset = 0) -> std::optional<int> {
        debug_report_rough_align_energy(label, decimation, dt_dec, threshold_high, offset, regions, num_regions, energy);
        return std::nullopt;
    };

    if (num_regions != 4) {
        return fail();
    }

    for (int i = 0; i < 4; ++i) {
        float detected = static_cast<float>(regions[i].end - regions[i].start) * dt_dec;
        float expected = (i < 2) ? profile.pass_time1() : profile.pass_time2();
        if (detected < expected * (1.0f - duration_tolerance)
            || detected > expected * (1.0f + duration_tolerance)) {
            return fail();
        }
    }

    // Average shift of region centers relative to expected peak times
    auto t_peaks = profile.expected_peak_times();
    float offset_sum = 0;
    for (int i = 0; i < 4; ++i) {
        float region_center_time = static_cast<float>(regions[i].start + regions[i].end) / 2.0f * dt_dec;
        offset_sum += (region_center_time - t_peaks[i]);
    }
    int offset_original = static_cast<int>((offset_sum / 4.0f) / dt);

    debug_report_rough_align_energy(label, decimation, dt_dec, threshold_high, offset_original, regions, num_regions, energy);
    return offset_original;
}

// Detect peaks in 4 passes of two-speed sweep motion
static FourPassPeaks detect_four_peaks(
    const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples,
    float dt,
    const SweepSpeedProfile &profile,
    uint32_t motion_profile_start_us,
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
    auto result = execute_motion_with_recording(
        std::move(motion_signal), sensor, label, expected_duration_us);

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

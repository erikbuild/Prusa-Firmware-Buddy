// Debug reporter functions for contactless offset measurement.
// Included directly into contactless_offset.cpp — not a standalone translation unit.
// All functions are called unconditionally; #ifdef SERIAL_DEBUG gates the bodies so we can
// always call them to not clutter the business logic with extra ifs.

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <option/has_usb_device.h>
#if HAS_USB_DEVICE()
    #include <USBSerial.h>
#endif

// serial_printf is only called from within #ifdef SERIAL_DEBUG bodies
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
    [[maybe_unused]] const char *label;
    [[maybe_unused]] bool first_sample = true;

public:
    explicit LineSamplesDebugReporter(const char *label)
        : label(label) {}

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

static void debug_report_scan_result([[maybe_unused]] const char *label, [[maybe_unused]] float confidence, float position_mm) {
#ifdef SERIAL_DEBUG
    serial_printf("# scan_result {\"label\": \"%s\", \"confidence\": %.3f, \"position_mm\": %.3f}\n", label, confidence, position_mm);
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

template <typename Container>
static void debug_report_symmetry_search(
    [[maybe_unused]] const Container &signal_value,
    [[maybe_unused]] const Container &signal_deriv,
    [[maybe_unused]] unsigned max_lag,
    [[maybe_unused]] int coarse_lag, [[maybe_unused]] float coarse_score,
    [[maybe_unused]] int fine_min, [[maybe_unused]] int fine_max,
    [[maybe_unused]] int best_lag, [[maybe_unused]] float best_combined) {
#ifdef SERIAL_DEBUG
    unsigned n = static_cast<unsigned>(signal_value.size());
    float corr_at_0_val = sp::symmetry_correlation(signal_value, 0, 1.0f);
    float corr_at_0_der = sp::symmetry_correlation(signal_deriv, 0, -1.0f);
    float corr_at_best_val = sp::symmetry_correlation(signal_value, best_lag, 1.0f);
    float corr_at_best_der = sp::symmetry_correlation(signal_deriv, best_lag, -1.0f);

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

    // Find per-type best lags
    int best_lag_value = fine_min, best_lag_deriv = fine_min;
    float best_val_score = -std::numeric_limits<float>::infinity();
    float best_der_score = -std::numeric_limits<float>::infinity();

    serial_printf("# pass_correlation {\"label\": \"%s\", \"pass\": %d, "
                  "\"fine_min\": %d, \"fine_max\": %d, ",
        label, pass_num, fine_min, fine_max);

    for (int lag = fine_min; lag <= fine_max; ++lag) {
        float cv = sp::symmetry_correlation(signal_value, lag, 1.0f) * inv_val;
        float cd = sp::symmetry_correlation(signal_deriv, lag, -1.0f) * inv_der;
        if (cv > best_val_score) {
            best_val_score = cv;
            best_lag_value = lag;
        }
        if (cd > best_der_score) {
            best_der_score = cd;
            best_lag_deriv = lag;
        }
    }

    serial_printf("\"best_lag_value\": %d, \"best_lag_deriv\": %d, \"best_lag_combined\": %d, ",
        best_lag_value, best_lag_deriv, best_lag);

    serial_printf("\"value_scores\": [");
    for (int lag = fine_min; lag <= fine_max; ++lag) {
        if (lag > fine_min) {
            serial_printf(", ");
        }
        serial_printf("%.4f", sp::symmetry_correlation(signal_value, lag, 1.0f) * inv_val);
    }

    serial_printf("], \"deriv_scores\": [");
    for (int lag = fine_min; lag <= fine_max; ++lag) {
        if (lag > fine_min) {
            serial_printf(", ");
        }
        serial_printf("%.4f", sp::symmetry_correlation(signal_deriv, lag, -1.0f) * inv_der);
    }

    serial_printf("], \"combined_scores\": [");
    for (int lag = fine_min; lag <= fine_max; ++lag) {
        if (lag > fine_min) {
            serial_printf(", ");
        }
        float cv = sp::symmetry_correlation(signal_value, lag, 1.0f) * inv_val;
        float cd = sp::symmetry_correlation(signal_deriv, lag, -1.0f) * inv_der;
        serial_printf("%.4f", cv + cd);
    }
    serial_printf("]}\n");
#endif
}

template <typename Container>
static void debug_report_rough_align_score(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] float dt_dec,
    [[maybe_unused]] int k_min,
    [[maybe_unused]] int k_max,
    [[maybe_unused]] int k_best,
    [[maybe_unused]] float score_over_baseline,
    [[maybe_unused]] const Container &score_curve) {
#ifdef SERIAL_DEBUG
    serial_printf("# rough_align_score {\"label\": \"%s\", \"dt_dec\": %.6f, "
                  "\"k_min\": %d, \"k_max\": %d, \"k_best\": %d, "
                  "\"score_over_baseline\": %.4f, \"score_curve\": [",
        label, dt_dec, k_min, k_max, k_best, score_over_baseline);
    for (size_t i = 0; i < score_curve.size(); ++i) {
        if (i > 0) {
            serial_printf(", ");
        }
        serial_printf("%.4e", static_cast<float>(score_curve[i]));
    }
    serial_printf("]}\n");
#endif
}

template <typename Container>
static void debug_report_rough_align_energy(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] int decimation,
    [[maybe_unused]] float dt_dec,
    [[maybe_unused]] float threshold,
    [[maybe_unused]] int offset_original,
    [[maybe_unused]] const EnergyRegion *regions,
    [[maybe_unused]] size_t num_regions,
    [[maybe_unused]] const Container &energy) {
#ifdef SERIAL_DEBUG
    serial_printf("# rough_align_energy {\"label\": \"%s\", \"decimation\": %d, "
                  "\"dt_dec\": %.6f, \"threshold\": %.6f, \"offset\": %d, \"regions\": [",
        label, decimation, dt_dec, threshold, offset_original);
    for (size_t i = 0; i < num_regions; ++i) {
        if (i > 0) {
            serial_printf(", ");
        }
        serial_printf("[%u, %u]",
            static_cast<unsigned>(regions[i].start),
            static_cast<unsigned>(regions[i].end));
    }
    serial_printf("], \"energy\": [");
    for (size_t i = 0; i < energy.size(); ++i) {
        if (i > 0) {
            serial_printf(", ");
        }
        serial_printf("%.4f", energy[i]);
    }
    serial_printf("]}\n");
#endif
}

// Reports the second-pass (trimmed) symmetry refinement.
// pass1_lag/pass1_score = full-signal first-pass result (integer lag, parabolic-refined value not used).
// pass2_lag/pass2_score = trimmed-signal second-pass result, in original-signal lag coordinates.
// pass1_refined_full / pass2_refined_full = parabolic-refined floats in original lag coordinates.
static void debug_report_pass_trim_refine(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] int pass_num,
    [[maybe_unused]] size_t n_full,
    [[maybe_unused]] size_t n_kept,
    [[maybe_unused]] size_t trim_start,
    [[maybe_unused]] int pass1_lag,
    [[maybe_unused]] float pass1_score,
    [[maybe_unused]] float pass1_refined,
    [[maybe_unused]] int pass2_lag,
    [[maybe_unused]] float pass2_score,
    [[maybe_unused]] float pass2_refined) {
#ifdef SERIAL_DEBUG
    serial_printf("# pass_trim_refine {\"label\": \"%s\", \"pass\": %d, "
                  "\"n_full\": %u, \"n_kept\": %u, \"trim_start\": %u, "
                  "\"pass1_lag\": %d, \"pass1_score\": %.4f, \"pass1_refined\": %.4f, "
                  "\"pass2_lag\": %d, \"pass2_score\": %.4f, \"pass2_refined\": %.4f}\n",
        label, pass_num,
        static_cast<unsigned>(n_full),
        static_cast<unsigned>(n_kept),
        static_cast<unsigned>(trim_start),
        pass1_lag, pass1_score, pass1_refined,
        pass2_lag, pass2_score, pass2_refined);
#endif
}

static void debug_report_pass_raw_chunk(
    [[maybe_unused]] const char *label,
    [[maybe_unused]] int pass_num,
    [[maybe_unused]] size_t chunk_start,
    [[maybe_unused]] size_t chunk_size,
    [[maybe_unused]] const sfl::segmented_vector<RawRecordedSample, 512> &raw_samples) {
#ifdef SERIAL_DEBUG
    serial_printf("# pass_raw_chunk {\"label\": \"%s\", \"pass\": %d, "
                  "\"chunk_start\": %u, \"chunk_size\": %u, \"samples\": [",
        label, pass_num,
        static_cast<unsigned>(chunk_start),
        static_cast<unsigned>(chunk_size));
    for (size_t i = 0; i < chunk_size; ++i) {
        if (i > 0) {
            serial_printf(", ");
        }
        serial_printf("%.0f", raw_samples[chunk_start + i].sensor_value);
    }
    serial_printf("]}\n");
#endif
}

static void debug_report_motion_profile(
    [[maybe_unused]] const SweepSpeedProfile &profile,
    [[maybe_unused]] sp::SamplingFreq sampling_freq,
    [[maybe_unused]] const char *label) {
#ifdef SERIAL_DEBUG
    const float dt = 1.0f / sampling_freq;
    auto profile_source = profile.make_source(sampling_freq);

    serial_printf("# motion_profile {\"label\": \"%s\", \"mode\": \"twospeed\", \"speed1\": %.2f, \"speed2\": %.2f, \"rest_time\": %.6f, "
                  "\"header\": \"time_s, position_mm, velocity_mm_s\", \"samples\": [",
        label, profile.speed1, profile.speed2, profile.rest_time);
    bool first = true;
    float pos = 0.0f;
    float t = 0.0f;
    while (sp::pipe::available(profile_source)) {
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
        label, static_cast<unsigned long>(result.convert_time_ms), static_cast<unsigned long>(result.steps_time_ms),
        static_cast<unsigned long>(result.expected_time_ms), static_cast<unsigned long>(result.actual_time_ms), sensor_offset_ms);
#endif
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
                      "\"correlation_peak\": %.3f, "
                      "\"correlation_peak_full\": %.3f}\n",
            label, name, i + 1,
            pass.peak_time_s,
            pass.confidence,
            pass.correlation_peak,
            pass.correlation_peak_full);
    }
#endif
}

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <signal_processing/analysis.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("linear_detrend removes linear ramp", "[signal_processing][analysis]") {
    std::vector<float> buf(100);
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = 3.0f + 0.5f * static_cast<float>(i);
    }

    sp::linear_detrend(buf);

    float sum = 0;
    for (auto v : buf) {
        sum += v;
    }
    REQUIRE_THAT(sum / static_cast<float>(buf.size()), WithinAbs(0.0, 1e-3));

    for (auto v : buf) {
        REQUIRE_THAT(v, WithinAbs(0.0, 1e-3));
    }
}

TEST_CASE("linear_detrend preserves zero-mean symmetric signal", "[signal_processing][analysis]") {
    std::vector<float> buf(100);
    for (size_t i = 0; i < buf.size(); ++i) {
        float x = static_cast<float>(i) / 99.0f - 0.5f;
        buf[i] = x * x; // parabola centered at 0.5
    }

    sp::linear_detrend(buf);

    // The linear fit of a parabola is a line, so the residuals should still be parabolic
    // After detrending, endpoints should have larger magnitude than center
    REQUIRE(std::abs(buf[0]) > std::abs(buf[50]));
}

TEST_CASE("normalize_inplace produces zero mean unit variance", "[signal_processing][analysis]") {
    std::vector<float> buf = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };

    auto [mean, std_dev] = sp::normalize_inplace(buf);

    REQUIRE_THAT(mean, WithinAbs(5.5, 1e-5));
    REQUIRE(std_dev > 0.0f);

    // Check output has zero mean
    float sum = 0;
    for (auto v : buf) {
        sum += v;
    }
    REQUIRE_THAT(sum / static_cast<float>(buf.size()), WithinAbs(0.0, 1e-5));

    // Check output has unit variance
    float sum_sq = 0;
    for (auto v : buf) {
        sum_sq += v * v;
    }
    float out_var = sum_sq / static_cast<float>(buf.size());
    REQUIRE_THAT(out_var, WithinAbs(1.0, 1e-4));
}

TEST_CASE("normalize_inplace with constant signal returns zero std", "[signal_processing][analysis]") {
    std::vector<float> buf(10, 5.0f);

    auto [mean, std_dev] = sp::normalize_inplace(buf);

    REQUIRE_THAT(mean, WithinAbs(5.0, 1e-5));
    REQUIRE_THAT(std_dev, WithinAbs(0.0, 1e-5));

    // Buffer should be unchanged (std < epsilon)
    for (auto v : buf) {
        REQUIRE_THAT(v, WithinAbs(5.0, 1e-5));
    }
}

TEST_CASE("symmetry_correlation peaks at lag=0 for symmetric signal", "[signal_processing][analysis]") {
    // Symmetric signal: Gaussian-like bell centered in the buffer
    std::vector<float> buf(101);
    float sigma = 15.0f;
    for (size_t i = 0; i < buf.size(); ++i) {
        float x = static_cast<float>(i) - 50.0f;
        buf[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
    }
    // Remove mean so correlation is well-conditioned
    sp::normalize_inplace(buf);

    float corr_at_0 = sp::symmetry_correlation(buf, 0, 1.0f);

    // Correlation at lag=0 should be higher than at any tested offset
    for (int lag = -20; lag <= 20; ++lag) {
        if (lag == 0) {
            continue;
        }
        float c = sp::symmetry_correlation(buf, lag, 1.0f);
        REQUIRE(corr_at_0 > c);
    }
}

TEST_CASE("symmetry_correlation with sign=-1 negates result", "[signal_processing][analysis]") {
    std::vector<float> buf = { 1.0f, 2.0f, 3.0f, 2.0f, 1.0f };

    float pos = sp::symmetry_correlation(buf, 0, 1.0f);
    float neg = sp::symmetry_correlation(buf, 0, -1.0f);

    REQUIRE_THAT(neg, WithinAbs(-pos, 1e-6));
}

TEST_CASE("parabolic_peak_offset finds exact peak", "[signal_processing][analysis]") {
    // Parabola y = -(x + 0.3)^2, sampled at x = -1, 0, 1
    // Peak at x = -0.3, so offset from center should be -0.3
    float y0 = -0.49f; // y(-1) = -(−1 + 0.3)² = −0.49
    float y1 = -0.09f; // y(0)  = −(0 + 0.3)²  = −0.09
    float y2 = -1.69f; // y(1)  = −(1 + 0.3)²  = −1.69

    float offset = sp::parabolic_peak_offset(y0, y1, y2);

    // Standard parabolic interpolation: negative offset means peak is toward y0
    REQUIRE_THAT(offset, WithinAbs(-0.3, 1e-4));
}

TEST_CASE("parabolic_peak_offset returns 0 for symmetric peak", "[signal_processing][analysis]") {
    float offset = sp::parabolic_peak_offset(1.0f, 3.0f, 1.0f);
    REQUIRE_THAT(offset, WithinAbs(0.0, 1e-6));
}

TEST_CASE("parabolic_peak_offset returns 0 for concave-up (valley)", "[signal_processing][analysis]") {
    // y0=2, y1=0, y2=2 is a valley (concave up) — not a peak
    float offset = sp::parabolic_peak_offset(2.0f, 0.0f, 2.0f);
    REQUIRE_THAT(offset, WithinAbs(0.0, 1e-6));
}

TEST_CASE("parabolic_peak_offset returns 0 for flat signal", "[signal_processing][analysis]") {
    float offset = sp::parabolic_peak_offset(5.0f, 5.0f, 5.0f);
    REQUIRE_THAT(offset, WithinAbs(0.0, 1e-6));
}

TEST_CASE("zero_phase_biquad produces zero phase shift", "[signal_processing][analysis]") {
    // Create a sinusoidal signal
    constexpr size_t n = 200;
    constexpr float fs = 1000.0f;
    constexpr float freq = 50.0f;
    std::vector<float> original(n);
    for (size_t i = 0; i < n; ++i) {
        original[i] = std::sin(2.0f * std::numbers::pi_v<float> * freq * static_cast<float>(i) / fs);
    }

    std::vector<float> filtered = original;
    auto coeffs = sp::butterworth_lowpass_biquad_2nd(200.0f, fs);
    sp::zero_phase_biquad(filtered, coeffs);

    // Find zero-crossings in the middle of the signal (avoid transients at edges)
    // A zero-phase filter should not shift the zero crossings
    auto find_zero_crossing = [](const std::vector<float> &sig, size_t start, size_t end) -> float {
        for (size_t i = start; i < end - 1; ++i) {
            if (sig[i] <= 0.0f && sig[i + 1] > 0.0f) {
                // Linear interpolation for sub-sample crossing
                float frac = -sig[i] / (sig[i + 1] - sig[i]);
                return static_cast<float>(i) + frac;
            }
        }
        return -1.0f;
    };

    float orig_crossing = find_zero_crossing(original, 50, 150);
    float filt_crossing = find_zero_crossing(filtered, 50, 150);

    REQUIRE(orig_crossing > 0.0f);
    REQUIRE(filt_crossing > 0.0f);
    // Zero-crossings should be at the same position (zero phase shift)
    REQUIRE_THAT(filt_crossing, WithinAbs(orig_crossing, 0.5));
}

// Edge case: empty and single-element inputs

TEST_CASE("linear_detrend handles empty and single-element buffers", "[signal_processing][analysis]") {
    std::vector<float> empty;
    sp::linear_detrend(empty); // should not crash

    std::vector<float> single = { 42.0f };
    sp::linear_detrend(single);
    REQUIRE_THAT(single[0], WithinAbs(42.0, 1e-6));
}

TEST_CASE("normalize_inplace handles empty buffer", "[signal_processing][analysis]") {
    std::vector<float> empty;
    auto [mean, std_dev] = sp::normalize_inplace(empty);
    REQUIRE_THAT(mean, WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(std_dev, WithinAbs(0.0, 1e-6));
}

TEST_CASE("normalize_inplace is stable with large DC offset", "[signal_processing][analysis]") {
    // Values near 1e6 with small variation — single-pass E[x²]-E[x]² would fail
    std::vector<float> buf(100);
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = 1e6f + 0.01f * static_cast<float>(i);
    }

    auto [mean, std_dev] = sp::normalize_inplace(buf);

    REQUIRE(std_dev > 0.0f);

    // Check output has approximately zero mean (float subtraction at 1e6 has ~0.06 ULP error)
    double sum = 0;
    for (auto v : buf) {
        sum += v;
    }
    REQUIRE_THAT(sum / static_cast<double>(buf.size()), WithinAbs(0.0, 0.05));

    // Check output has approximately unit variance
    double sum_sq = 0;
    for (auto v : buf) {
        sum_sq += static_cast<double>(v) * v;
    }
    double out_var = sum_sq / static_cast<double>(buf.size());
    REQUIRE_THAT(out_var, WithinAbs(1.0, 0.05));
}

TEST_CASE("symmetry_correlation returns 0 for empty buffer", "[signal_processing][analysis]") {
    std::vector<float> empty;
    REQUIRE_THAT(sp::symmetry_correlation(empty, 0), WithinAbs(0.0, 1e-6));
}

TEST_CASE("symmetry_correlation with large lag returns 0", "[signal_processing][analysis]") {
    std::vector<float> buf = { 1.0f, 2.0f, 3.0f };
    // Lag larger than buffer — no overlapping indices
    REQUIRE_THAT(sp::symmetry_correlation(buf, 100), WithinAbs(0.0, 1e-6));
}

TEST_CASE("parabolic_peak_offset with positive offset", "[signal_processing][analysis]") {
    // Parabola y = -(x - 0.4)^2, sampled at x = -1, 0, 1
    // Peak at x = 0.4 (positive offset toward y2)
    float y0 = -((-1.0f - 0.4f) * (-1.0f - 0.4f)); // -1.96
    float y1 = -((0.0f - 0.4f) * (0.0f - 0.4f)); // -0.16
    float y2 = -((1.0f - 0.4f) * (1.0f - 0.4f)); // -0.36

    float offset = sp::parabolic_peak_offset(y0, y1, y2);
    REQUIRE_THAT(offset, WithinAbs(0.4, 1e-4));
}

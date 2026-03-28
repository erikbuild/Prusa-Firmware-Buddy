#pragma once

#include <signal_processing/filters.hpp>

#include <cmath>
#include <cstddef>
#include <utility>

namespace sp {

// Apply biquad filter forward then backward in-place, cancelling phase shift.
// The filter is reset between passes, so edge transients affect roughly
// 2*filter_order samples at each end. Callers should pad or trim edges
// if clean boundary behavior is needed. Magnitude response is squared.
template <typename Container>
void zero_phase_biquad(Container &buf, const BiquadCoeffs<float> &coeffs) {
    const auto n = buf.size();
    if (n == 0) {
        return;
    }

    Biquad<float> filt(coeffs);

    // Forward pass
    for (size_t i = 0; i < n; ++i) {
        buf[i] = filt.process(buf[i]);
    }

    // Backward pass (reset filter state to avoid bleeding)
    filt.reset();
    for (size_t i = n; i-- > 0;) {
        buf[i] = filt.process(buf[i]);
    }
}

// returns {intercept, slope}, or nullopt if degenerate
template <typename Container>
inline std::optional<std::pair<float, float>> least_squares_line(const Container &buf) {
    const auto n = buf.size();
    if (n < 2) {
        return std::nullopt;
    }

    float n_f = static_cast<float>(n);
    float sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = 0; i < n; ++i) {
        float x = static_cast<float>(i);
        float y = buf[i];
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }

    float denom = n_f * sxx - sx * sx;
    if (std::abs(denom) < 1e-10f) {
        return std::nullopt;
    }

    float b = (n_f * sxy - sx * sy) / denom;
    float a = (sy - b * sx) / n_f;
    return std::make_pair(a, b);
}

// Remove least-squares linear fit from buffer in-place.
// After this, the signal has no linear trend component.
template <typename Container>
void linear_detrend(Container &buf) {
    if (auto ab = least_squares_line(buf)) {
        auto [a, b] = *ab;
        for (size_t i = 0; i < buf.size(); ++i) {
            buf[i] -= a + b * i;
        }
    }
}

// Normalize buffer to zero mean and unit variance in-place.
// Returns {mean, std_dev}. If std_dev < epsilon, buffer is unchanged.
// Uses two-pass algorithm to avoid catastrophic cancellation with large offsets.
template <typename Container>
std::pair<float, float> normalize_inplace(Container &buf, float epsilon = 1e-10f) {
    const auto n = buf.size();
    if (n == 0) {
        return { 0.0f, 0.0f };
    }

    // Pass 1: compute mean
    double sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += static_cast<double>(buf[i]);
    }
    float mean = static_cast<float>(sum / static_cast<double>(n));

    // Pass 2: compute variance from centered data
    double sum_sq = 0;
    for (size_t i = 0; i < n; ++i) {
        double d = static_cast<double>(buf[i]) - static_cast<double>(mean);
        sum_sq += d * d;
    }
    float std_dev = static_cast<float>(std::sqrt(sum_sq / static_cast<double>(n)));

    if (std_dev < epsilon) {
        return { mean, std_dev };
    }

    for (size_t i = 0; i < n; ++i) {
        buf[i] = (buf[i] - mean) / std_dev;
    }

    return { mean, std_dev };
}

// Zero-phase biquad filtering with odd-extension edge padding.
// Pads with odd-extended samples at both ends to eliminate
// edge transients, then applies forward-backward filtering.
// Pad length is min(10*tau, n-1), where tau is the approximate
// first-order time constant sample_rate / (2*pi*cutoff_hz).
template <typename Container, typename WorkBuffer = Container>
void zero_phase_biquad_padded(Container &buf, const BiquadCoeffs<float> &coeffs,
    float sample_rate, float cutoff_hz) {

    const size_t n = buf.size();
    if (n < 2) {
        return;
    }

    const size_t tau_samples = static_cast<size_t>(
        sample_rate / (2.0f * static_cast<float>(M_PI) * cutoff_hz));
    const size_t pad_len = std::min(10 * tau_samples, n - 1);

    WorkBuffer padded;
    padded.reserve(n + 2 * pad_len);

    // Front: odd extension around buf[0]
    for (size_t i = pad_len; i >= 1; --i) {
        padded.push_back(2.0f * buf[0] - buf[i]);
    }
    for (size_t i = 0; i < n; ++i) {
        padded.push_back(buf[i]);
    }
    // Back: odd extension around buf[n-1]
    for (size_t i = 1; i <= pad_len; ++i) {
        padded.push_back(2.0f * buf[n - 1] - buf[n - 1 - i]);
    }

    const size_t pn = padded.size();
    Biquad<float> filt(coeffs);
    for (size_t i = 0; i < pn; ++i) {
        padded[i] = filt.process(padded[i]);
    }
    filt.reset();
    for (size_t i = pn; i-- > 0;) {
        padded[i] = filt.process(padded[i]);
    }

    for (size_t i = 0; i < n; ++i) {
        buf[i] = padded[pad_len + i];
    }
}

// Downsample a signal by averaging blocks of consecutive samples.
// Output size = in.size() / factor (remainder samples are discarded).
template <typename InContainer, typename OutContainer>
void decimate_average(const InContainer &in, OutContainer &out, size_t factor) {
    const size_t n_out = in.size() / factor;
    out.reserve(out.size() + n_out);
    for (size_t i = 0; i < n_out; ++i) {
        float sum = 0;
        for (size_t j = 0; j < factor; ++j) {
            sum += in[i * factor + j];
        }
        out.push_back(sum / static_cast<float>(factor));
    }
}

// Unnormalized correlation of signal with its reverse at a given lag.
// corr(lag) = sign * sum(buf[i] * buf[n-1-i-lag])
// Peaks at lag=0 for symmetric signals. Use sign=-1 to negate the result
// (convenient for combining value and derivative symmetry scores).
// NOT divided by overlap count — this prevents extreme lags with tiny
// overlap from producing spuriously high scores.
template <typename Container>
float symmetry_correlation(const Container &buf, int lag, float sign = 1.0f) {
    int n = static_cast<int>(buf.size());
    float corr = 0;

    for (int i = 0; i < n; ++i) {
        int j = (n - 1 - i) - lag;
        if (j >= 0 && j < n) {
            corr += buf[i] * buf[j];
        }
    }

    return sign * corr;
}

// Sub-sample peak refinement via parabolic interpolation.
// Given three equally-spaced values y0, y1, y2 at positions x-1, x, x+1,
// returns the fractional offset from x of the interpolated peak.
// Positive offset means the peak is between x and x+1 (toward y2).
// Returns 0.0 if the parabola is degenerate (flat) or concave up (no peak).
inline float parabolic_peak_offset(float y0, float y1, float y2) {
    float denom = y0 - 2.0f * y1 + y2;
    // denom < 0 means concave-down (peak); >= 0 means valley or flat
    if (denom > -1e-10f) {
        return 0.0f;
    }
    return (y0 - y2) / (2.0f * denom);
}

} // namespace sp

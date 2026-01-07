#include <signal_processing/transfer.hpp>

#include <iostream>
#include <vector>
#include <random>
#include <numbers>
#include <cmath>

namespace {

struct Biquad {
    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

    float x1 = 0.0f;
    float x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;

    float process(float x) {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

Biquad make_lowpass(float sampling_freq, float cutoff_freq, float q) {
    const float w0 = 2.0f * std::numbers::pi_v<float> * cutoff_freq / sampling_freq;
    const float cw = std::cos(w0);
    const float sw = std::sin(w0);
    const float alpha = sw / (2.0f * q);

    const float b0 = (1.0f - cw) * 0.5f;
    const float b1 = 1.0f - cw;
    const float b2 = (1.0f - cw) * 0.5f;
    const float a0 = 1.0f + alpha;
    const float a1 = -2.0f * cw;
    const float a2 = 1.0f - alpha;

    Biquad biquad;
    biquad.b0 = b0 / a0;
    biquad.b1 = b1 / a0;
    biquad.b2 = b2 / a0;
    biquad.a1 = a1 / a0;
    biquad.a2 = a2 / a0;
    return biquad;
}

float chirp_sample(size_t i, float sampling_freq, float f0, float f1, float duration) {
    const float t = static_cast<float>(i) / sampling_freq;
    const float k = (f1 - f0) / duration;
    const float phase = 2.0f * std::numbers::pi_v<float> * (f0 * t + 0.5f * k * t * t);
    return std::sin(phase);
}

} // namespace

int main() {
    constexpr float SAMPLING_FREQ = 1000.0f;
    constexpr size_t WINDOW_SIZE = 512;
    constexpr float OVERLAP = 0.5f;
    constexpr size_t TOTAL_SAMPLES = 20000;

    constexpr float CUTOFF_FREQ = 180.0f;
    constexpr float Q = 0.707f;

    sp::H1TransferEstimator<> noise_estimator(WINDOW_SIZE, OVERLAP);
    sp::H1TransferEstimator<> chirp_estimator(WINDOW_SIZE, OVERLAP);

    Biquad noise_filter = make_lowpass(SAMPLING_FREQ, CUTOFF_FREQ, Q);
    Biquad chirp_filter = make_lowpass(SAMPLING_FREQ, CUTOFF_FREQ, Q);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<float> noise_in(TOTAL_SAMPLES);
    std::vector<float> noise_out(TOTAL_SAMPLES);
    std::vector<float> chirp_in(TOTAL_SAMPLES);
    std::vector<float> chirp_out(TOTAL_SAMPLES);

    for (size_t i = 0; i < TOTAL_SAMPLES; ++i) {
        const float x = dist(rng);
        const float y = noise_filter.process(x);
        noise_in[i] = x;
        noise_out[i] = y;
        noise_estimator.push(x, y);
    }

    const float duration = static_cast<float>(TOTAL_SAMPLES) / SAMPLING_FREQ;
    constexpr float f0 = 2.0f;
    const float f1 = SAMPLING_FREQ * 0.45f;
    for (size_t i = 0; i < TOTAL_SAMPLES; ++i) {
        const float x = chirp_sample(i, SAMPLING_FREQ, f0, f1, duration);
        const float y = chirp_filter.process(x);
        chirp_in[i] = x;
        chirp_out[i] = y;
        chirp_estimator.push(x, y);
    }

    auto noise_tf = noise_estimator.finalize(SAMPLING_FREQ);
    auto chirp_tf = chirp_estimator.finalize(SAMPLING_FREQ);

    std::cout << "# SAMPLING_FREQ=" << SAMPLING_FREQ << "\n";
    std::cout << "# WINDOW_SIZE=" << WINDOW_SIZE << "\n";
    std::cout << "# OVERLAP=" << OVERLAP << "\n";
    std::cout << "# CUTOFF_FREQ=" << CUTOFF_FREQ << "\n";
    std::cout << "# Q=" << Q << "\n";
    std::cout << "# TOTAL_SAMPLES=" << TOTAL_SAMPLES << "\n";
    std::cout << "# B0=" << noise_filter.b0 << "\n";
    std::cout << "# B1=" << noise_filter.b1 << "\n";
    std::cout << "# B2=" << noise_filter.b2 << "\n";
    std::cout << "# A1=" << noise_filter.a1 << "\n";
    std::cout << "# A2=" << noise_filter.a2 << "\n";

    std::cout << "Index,NoiseIn,NoiseOut,ChirpIn,ChirpOut\n";
    for (size_t i = 0; i < TOTAL_SAMPLES; ++i) {
        std::cout << i << "," << noise_in[i] << "," << noise_out[i] << ","
                  << chirp_in[i] << "," << chirp_out[i] << "\n";
    }

    std::cout << "# END_SAMPLES\n";
    std::cout << "Freq,NoiseRe,NoiseIm,ChirpRe,ChirpIm\n";
    for (size_t k = 0; k < noise_tf.size(); ++k) {
        const float freq = noise_tf.get_frequency(k);
        const auto n = noise_tf[k];
        const auto c = chirp_tf[k];
        std::cout << freq << "," << n.re << "," << n.im << "," << c.re << "," << c.im << "\n";
    }

    return 0;
}

#include <signal_processing/pipeline.hpp>
#include <signal_processing/filters.hpp>

#include <cstdint>
#include <vector>
#include <ranges>
#include <numbers>
#include <cmath>
#include <iostream>

int main() {
    constexpr float SAMPLING_FREQ = 1000.f;
    constexpr float CUTOFF_FREQ = 10.f;
    constexpr size_t TEST_SAMPLES = 10000;

    // Generate a quadratic chirp signal
    std::vector<float> input_signal(TEST_SAMPLES);
    constexpr float f0 = 1.0f;
    constexpr float f1 = SAMPLING_FREQ / 2.0f;
    constexpr float duration = TEST_SAMPLES / SAMPLING_FREQ;
    for (size_t i = 0; i < TEST_SAMPLES; ++i) {
        float t = static_cast<float>(i) / SAMPLING_FREQ;
        float phase = 2.0f * std::numbers::pi_v<float> * (f0 * t + (f1 - f0) * t * t * t / (3.0f * duration * duration));
        input_signal[i] = std::sin(phase);
    }

    // Run it through various low-pass filters
    using namespace sp::pipe;

    auto hamming_pipeline = make_source(input_signal, SAMPLING_FREQ)
        | hamming_lowpass<float, 21>(CUTOFF_FREQ);
    auto gaussian_pipeline = make_source(input_signal, SAMPLING_FREQ)
        | gaussian_lowpass_fc<float, 21>(CUTOFF_FREQ);
    auto butterworth_pipeline_1 = make_source(input_signal, SAMPLING_FREQ)
        | butterworth_lowpass_1st<float>(CUTOFF_FREQ);
    auto butterworth_pipeline_2 = make_source(input_signal, SAMPLING_FREQ)
        | butterworth_lowpass_2nd<float>(CUTOFF_FREQ);
    auto butterworth_pipeline_4 = make_source(input_signal, SAMPLING_FREQ)
        | butterworth_lowpass_4th<float>(CUTOFF_FREQ);

    // Output metadata for Python script
    std::cout << "# SAMPLING_FREQ=" << SAMPLING_FREQ << "\n";
    std::cout << "# CUTOFF_FREQ=" << CUTOFF_FREQ << "\n";

    // Output CSV data
    std::cout << "Input,Hamming,Gaussian,Butterworth1st,Butterworth2nd,Butterworth4th\n";
    for (size_t i = 0; i < TEST_SAMPLES; ++i) {
        float in = input_signal[i];
        float hamming_out = hamming_pipeline.next();
        float gaussian_out = gaussian_pipeline.next();
        float butter1_out = butterworth_pipeline_1.next();
        float butter2_out = butterworth_pipeline_2.next();
        float butter4_out = butterworth_pipeline_4.next();

        std::cout << in << ","
                  << hamming_out << ","
                  << gaussian_out << ","
                  << butter1_out << ","
                  << butter2_out << ","
                  << butter4_out << "\n";
    }
    return 0;
}

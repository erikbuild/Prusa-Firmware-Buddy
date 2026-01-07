#include <signal_processing/pipeline.hpp>
#include <signal_processing/filters.hpp>

#include <chrono>
#include <cstdint>
#include <vector>
#include <numbers>
#include <cmath>
#include <iostream>

int main() {
    constexpr float SAMPLING_FREQ = 1000.f;
    constexpr size_t TEST_SAMPLES = 5000;

    // Generate a signal with varying amplitude
    // First half: amplitude 0.2, Second half: amplitude 1.0
    std::vector<float> input_signal(TEST_SAMPLES);
    constexpr float freq = 10.f; // 10 Hz sine wave
    for (size_t i = 0; i < TEST_SAMPLES; ++i) {
        float t = static_cast<float>(i) / SAMPLING_FREQ;
        float amplitude = (i < TEST_SAMPLES / 2) ? 0.2f : 1.0f;
        input_signal[i] = amplitude * std::sin(2.0f * std::numbers::pi_v<float> * freq * t);
    }

    // Apply AGC to normalize to target RMS
    using namespace sp::pipe;

    constexpr float TARGET_RMS = 0.5f;
    const auto ATTACK_TIME = std::chrono::milliseconds(100);
    const auto RELEASE_TIME = std::chrono::milliseconds(100);

    auto agc_pipeline = make_source(input_signal, SAMPLING_FREQ)
        | agc(TARGET_RMS, ATTACK_TIME, RELEASE_TIME);

    // Output metadata
    std::cout << "# SAMPLING_FREQ=" << SAMPLING_FREQ << "\n";
    std::cout << "# TARGET_RMS=" << TARGET_RMS << "\n";
    const auto attack_seconds = std::chrono::duration_cast<std::chrono::duration<float>>(ATTACK_TIME).count();
    const auto release_seconds = std::chrono::duration_cast<std::chrono::duration<float>>(RELEASE_TIME).count();
    std::cout << "# ATTACK_TIME_SEC=" << attack_seconds << "\n";
    std::cout << "# RELEASE_TIME_SEC=" << release_seconds << "\n";

    // Output CSV data
    std::cout << "Sample,Input,Output,Gain,CurrentRMS\n";
    for (size_t i = 0; i < TEST_SAMPLES; ++i) {
        float in = input_signal[i];

        // Get the filter for inspection
        auto &filter = agc_pipeline.get_filter();

        float out = agc_pipeline.next();
        float gain = filter.get_current_gain();
        float current_rms = filter.get_current_rms();

        std::cout << i << ","
                  << in << ","
                  << out << ","
                  << gain << ","
                  << current_rms << "\n";
    }

    return 0;
}

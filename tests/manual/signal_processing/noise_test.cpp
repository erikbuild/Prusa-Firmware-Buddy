#include <signal_processing/pipeline.hpp>
#include <signal_processing/filters.hpp>
#include <signal_processing/generators.hpp>

#include <iostream>
#include <cmath>

int main() {
    constexpr float SAMPLING_FREQ = 1000.f;
    constexpr size_t TEST_SAMPLES = 10000;
    constexpr float TARGET_RMS = 0.5f;
    constexpr uint32_t SEED = 42; // Deterministic seed for reproducibility

    using namespace sp::pipe;

    // Create Gaussian white noise with deterministic seed
    auto noise = make_gaussian_noise<float>(TARGET_RMS, SEED, SAMPLING_FREQ);

    // You can also use it in a pipeline with filters
    auto filtered_noise = make_gaussian_noise<float>(TARGET_RMS, SEED + 1, SAMPLING_FREQ)
        | butterworth_lowpass_2nd<float>(50.0f);

    // Output metadata
    std::cout << "# SAMPLING_FREQ=" << SAMPLING_FREQ << "\n";
    std::cout << "# TEST_SAMPLES=" << TEST_SAMPLES << "\n";
    std::cout << "# TARGET_RMS=" << TARGET_RMS << "\n";
    std::cout << "# SEED=" << SEED << "\n";

    // Output CSV data
    std::cout << "Sample,Time,Noise,Filtered\n";

    // Calculate actual RMS while generating
    float sum_squares = 0.0f;

    for (size_t i = 0; i < TEST_SAMPLES; ++i) {
        float time = static_cast<float>(i) / SAMPLING_FREQ;
        float noise_value = noise.next();
        float filtered_value = filtered_noise.next();

        sum_squares += noise_value * noise_value;

        std::cout << i << ","
                  << time << ","
                  << noise_value << ","
                  << filtered_value << "\n";
    }

    // Verify RMS (output to stderr so it doesn't interfere with CSV)
    float actual_rms = std::sqrt(sum_squares / TEST_SAMPLES);
    std::cerr << "Target RMS: " << TARGET_RMS << "\n";
    std::cerr << "Actual RMS: " << actual_rms << "\n";
    std::cerr << "Difference: " << std::abs(actual_rms - TARGET_RMS) << " ("
              << (std::abs(actual_rms - TARGET_RMS) / TARGET_RMS * 100.0f) << "%)\n";

    return 0;
}

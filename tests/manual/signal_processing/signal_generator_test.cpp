#include <signal_processing/pipeline.hpp>
#include <signal_processing/filters.hpp>
#include <signal_processing/generators.hpp>

#include <chrono>
#include <iostream>
#include <vector>

int main() {
    constexpr float SAMPLING_FREQ = 1000.f;
    const auto DURATION = std::chrono::seconds(5);

    // Generate a linear chirp from 1 Hz to 100 Hz
    constexpr float START_FREQ = 1.0f;
    constexpr float END_FREQ = 100.0f;
    constexpr float AMPLITUDE = 1.0f;

    using namespace sp::pipe;

    // Create both linear and quadratic chirp sources
    auto linear_chirp = make_linear_chirp<float>(START_FREQ, END_FREQ, AMPLITUDE, DURATION, SAMPLING_FREQ);
    auto quadratic_chirp = make_quadratic_chirp<float>(START_FREQ, END_FREQ, AMPLITUDE, DURATION, SAMPLING_FREQ);

    // Output metadata
    std::cout << "# SAMPLING_FREQ=" << SAMPLING_FREQ << "\n";
    const auto duration_seconds = std::chrono::duration_cast<std::chrono::duration<float>>(DURATION).count();
    std::cout << "# DURATION=" << duration_seconds << "\n";
    std::cout << "# START_FREQ=" << START_FREQ << "\n";
    std::cout << "# END_FREQ=" << END_FREQ << "\n";
    std::cout << "# AMPLITUDE=" << AMPLITUDE << "\n";

    // Output CSV data
    std::cout << "Sample,Time,Linear,Quadratic\n";

    std::size_t sample_idx = 0;
    while (linear_chirp.poll() != sp::pipe::PollResult::done) {
        float time = static_cast<float>(sample_idx) / SAMPLING_FREQ;
        float linear_value = linear_chirp.next();
        float quadratic_value = quadratic_chirp.next();

        std::cout << sample_idx << ","
                  << time << ","
                  << linear_value << ","
                  << quadratic_value << "\n";

        ++sample_idx;
    }

    return 0;
}

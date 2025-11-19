#pragma once

#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <inplace_function.hpp>

namespace precise_timing {

using Frequency = uint32_t;

static constexpr Frequency counter_period = 48'000'000;

/// Helper class to convert time from internal counter into real timestamps
class Timer {
public:
    /// Gets timestamp in s.
    [[nodiscard]] uint32_t get_timestamp_s() const;
    /// Gets timestamp in ms.
    [[nodiscard]] uint64_t get_timestamp_ms() const;
    /// Gets timestamp in us.
    [[nodiscard]] uint64_t get_timestamp_us() const;
    /// Gets timestamp in ns.
    ///
    /// @warning Depending on mcu and it's internal clock speed, the measurement might not be accurate enough for long measurements.
    [[nodiscard]] uint64_t get_timestamp_ns() const;

    [[nodiscard]] Frequency get_counter_period() const;

    /// IRQ callback - when timer overflows
    void increment_seconds();

private:
    template <uint64_t time_scale>
    uint64_t calc_units(uint64_t seconds) const;

    template <uint64_t time_scale>
    uint64_t calc_timestamp_with_overflow_check() const;

    std::atomic<uint32_t> seconds = 0;
};

} // namespace precise_timing

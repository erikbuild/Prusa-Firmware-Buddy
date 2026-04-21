/// @file
#include "hotend_temp_compensation.hpp"

#include <atomic>
#include <freertos/timing.hpp>
#include <fpm/math.hpp>

namespace hotend_temp_compensation {

namespace {
    using FixedPoint = fpm::fixed_16_16;

    constexpr FixedPoint fadeout_ms { 6000 };

    std::atomic<FixedPoint> current_compensation = FixedPoint { 0 };
    std::atomic<FixedPoint> target_compensation = FixedPoint { 0 };
    uint32_t last_step_ms = 0;
} // namespace

/// Minium possible fadeout coef of for 1 ms
static constexpr auto minimum_fadeout_coef = fpm::exp(FixedPoint { -1 } / fadeout_ms);

/// Make sure the coef has not been rounded up to 1
static_assert(float(minimum_fadeout_coef) < 1);

/// Make sure the coef is reasonably precise
static_assert(std::abs(float(minimum_fadeout_coef) - expf(-1.0f / float(fadeout_ms))) < 0.00001f);

void step() {
    const auto now_ms = freertos::millis();
    const auto delta_t_ms = now_ms - last_step_ms;
    last_step_ms = now_ms;

    const FixedPoint fadeout_coef = fpm::exp(-FixedPoint { delta_t_ms } / fadeout_ms);
    const FixedPoint curr = current_compensation.load();
    current_compensation = curr * fadeout_coef + (FixedPoint { 1 } - fadeout_coef) * target_compensation.load();
}

void set_target_compensation_c100(int16_t set) {
    target_compensation = FixedPoint { set };
}

int16_t get_current_compensation_c100() {
    return int16_t { current_compensation.load() };
}

} // namespace hotend_temp_compensation

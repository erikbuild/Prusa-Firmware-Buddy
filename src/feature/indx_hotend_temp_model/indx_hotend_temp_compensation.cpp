/// @file
#include "indx_hotend_temp_compensation.hpp"

#include <algorithm>

#include <utils/math/ema.hpp>

namespace indx_hotend_temp_compensation {

namespace {

    // thermal resistances and constants used for calculations
    // See BFW-8630 for more info

    /// K/W at full fan power, thermal resistance of nozzle tip surface to air
    constexpr float nozzle_tip_thermal_resistance_k_w = 120;

    /// K/W, thermal resistance between the nozzle and the heatsink
    constexpr float heatbreak_thermal_resistance_k_w = 80;

    /// K/W/mm, thermal resistance of the nozzle body per mm, used for calculating heat gradient
    constexpr float nozzle_linear_thermal_resistance_k_w_mm = 1.7f;

    /// J/K, nozzle body thermal capacity
    constexpr float heat_capacity_j_k = 1.17f;

    /// Decides how much of temperature gradient is compensated [%]
    constexpr float compensation_factor = 0.3f;

    // Positions of heat sources and sinks
    constexpr float heatbreak_position_mm = 0;
    constexpr float nozzle_tip_position_mm = 28;
    constexpr float heat_capacity_center_mm = 11.2f;

    /// Position of the temperature sensor on the nozzle (that provides `hotend_temp_readout_c`)
    constexpr float temp_sensor_position_mm = 11;

    /// Base constant for calculating heat gradient from print fan
    constexpr float fan_offset_weight = (nozzle_tip_position_mm - temp_sensor_position_mm) * nozzle_linear_thermal_resistance_k_w_mm / nozzle_tip_thermal_resistance_k_w;

    /// Base constant for calculating heat gradient from heatsink
    constexpr float heatbreak_offset_weight = (heatbreak_position_mm - temp_sensor_position_mm) * nozzle_linear_thermal_resistance_k_w_mm / heatbreak_thermal_resistance_k_w;

    /// Base constant for calculating heat gradient from nozzle heating
    constexpr float heating_offset_weight = (heat_capacity_center_mm - temp_sensor_position_mm) * heat_capacity_j_k * nozzle_linear_thermal_resistance_k_w_mm;

} // namespace

FilamentPrecomputedParameters FilamentPrecomputedParameters::compute(const FilamentParameters &params) {
    static constexpr float base_const_coef = -54.71f * 0.001f;
    static constexpr float base_linear_coef = 2.2f * 0.001f;
    static constexpr float base_threshold = 6.57f;

    return FilamentPrecomputedParameters {
        .const_coef = base_const_coef * params.heat_per_mm / params.heat_time_constant,
        .linear_coef = base_linear_coef * params.heat_per_mm,
        .feedrate_threshold_mm_s = base_threshold / params.heat_time_constant,
        .heat_time_constant = params.heat_time_constant,
    };
}

void HotendTempCompensator::set_filament_parameters(const FilamentParameters &set) {
    set_filament_parameters(FilamentPrecomputedParameters::compute(set));
}

void HotendTempCompensator::set_filament_parameters(const FilamentPrecomputedParameters &set) {
    filament_ = set;
    reset_state();
}

float HotendTempCompensator::step(const StepParams &params) {
    const float hotend_ambient_temp_diff_c = params.hotend_temp_readout_c - params.chamber_temperature_c;

    // Apply ema to filament feedrate
    state_.feedrate_mm_s = exponential_moving_average(state_.feedrate_mm_s, params.extruder_feedrate_mm_s, params.dt_s, filament_.heat_time_constant);

    // Calculate filament offset
    float filament_offset_c;
    {
        const float effective_feedrate = std::max(state_.feedrate_mm_s, filament_.feedrate_threshold_mm_s);
        filament_offset_c = hotend_ambient_temp_diff_c * (filament_.linear_coef * effective_feedrate + filament_.const_coef);
        filament_offset_c = std::min<float>(filament_offset_c, 0);
    }

    // Calculate fan offset
    const float fan_offset_c = fan_offset_weight * sqrtf(params.print_fan_pwm / 255.0f) * hotend_ambient_temp_diff_c;

    // Calculate heatbreak offset
    const float heatbreak_offset_c = heatbreak_offset_weight * hotend_ambient_temp_diff_c;

    // Calculate heating offset
    const float heating_offset_c = heating_offset_weight * params.hotend_temp_readout_dt_c_s;

    // Intentionally without filament offset, that is computed separately
    const float offset_sum_c = fan_offset_c + heating_offset_c + heatbreak_offset_c;

    const float abs_filament_offset_c = std::abs(filament_offset_c);
    const float filament_offset_divisor_c = abs_filament_offset_c + std::abs(offset_sum_c);
    const float effective_filament_offset =
        // Protect from division by zero.
        // Because the dividend is basically ^2, it will always go to zero faster than divisor.
        // So in the limit, the result should approach zero anyway.
        (filament_offset_divisor_c == 0) ? 0.f : filament_offset_c * abs_filament_offset_c / filament_offset_divisor_c;

    const float final_temp_offset_c = compensation_factor * (effective_filament_offset + offset_sum_c);

    // Note: Exponential fadeout is then applied directly on the INDX head
    // Clamp to sane values, just in case
    return std::clamp(final_temp_offset_c, -80.f, 80.f);
}

void HotendTempCompensator::reset_state() {
    state_ = {};
}
} // namespace indx_hotend_temp_compensation

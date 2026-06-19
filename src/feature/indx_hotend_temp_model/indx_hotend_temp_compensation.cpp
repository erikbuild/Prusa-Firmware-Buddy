/// @file
#include "indx_hotend_temp_compensation.hpp"

#include <algorithm>

#include <utils/math/ema.hpp>

namespace indx_hotend_temp_compensation {

namespace {

    // thermal resistances and constants used for calculations
    // See BFW-8630 for more info

    /// K/W at full fan power, thermal resistance of nozzle tip surface to air
    constexpr float nozzle_tip_thermal_resistance_k_w = 200;

    /// K/W, thermal resistance between the nozzle and the heatsink
    constexpr float heatbreak_thermal_resistance_k_w = 160;

    /// K/W/mm, thermal resistance of the nozzle body per mm, used for calculating heat gradient
    constexpr float nozzle_linear_thermal_resistance_k_w_mm = 1.7f;

    /// J/K, nozzle body thermal capacity
    constexpr float heat_capacity_j_k = 1.17f;

    /// Decides how much of temperature gradient is compensated [%]
    constexpr float compensation_factor = 0.15f;

    // Positions of heat sources and sinks
    constexpr float heatbreak_position_mm = 0;
    constexpr float nozzle_tip_position_mm = 28;
    constexpr float heat_capacity_center_mm = 11.5f;

    /// Position of the temperature sensor on the nozzle (that provides `hotend_temp_readout_c`)
    constexpr float temp_sensor_position_mm = 11;

    /// Base constant for calculating heat gradient from print fan
    constexpr float fan_offset_weight = (nozzle_tip_position_mm - temp_sensor_position_mm) * nozzle_linear_thermal_resistance_k_w_mm / nozzle_tip_thermal_resistance_k_w;

    /// Base constant for calculating heat gradient from heatsink
    constexpr float heatbreak_offset_weight = (heatbreak_position_mm - temp_sensor_position_mm) * nozzle_linear_thermal_resistance_k_w_mm / heatbreak_thermal_resistance_k_w;

    /// Base constant for calculating heat gradient from nozzle heating
    constexpr float heating_offset_weight = (heat_capacity_center_mm - temp_sensor_position_mm) * heat_capacity_j_k * nozzle_linear_thermal_resistance_k_w_mm;

    constexpr float filament_base_linear_coef = 2.5f * 0.001f;
    constexpr float filament_const_coef = -0.12f;
    constexpr float filament_base_threshold = 28.f;
} // namespace

float HotendTempCompensator::step(const StepParams &params) {
    const float hotend_ambient_temp_diff_c = params.hotend_temp_readout_c - params.chamber_temperature_c;

    // Apply EMA to filament feedrate, time constant of 1 second was measured to be close to feedrate transients
    state_.feedrate_mm_s = exponential_moving_average(state_.feedrate_mm_s, params.extruder_feedrate_mm_s, params.dt_s, 1);

    // Calculate filament offset
    // Below threshold: constant = const_coef × ΔT
    // Above threshold: linearly approaches zero as feedrate increases
    float filament_offset_c = 0;
    {
        float slope = 0;
        if (params.filament.linear_heat_capacity_J_C_m > 0) {
            slope = std::max<float>(state_.feedrate_mm_s - filament_base_threshold / params.filament.linear_heat_capacity_J_C_m, 0);
        }
        const float coef = filament_const_coef + slope * filament_base_linear_coef * params.filament.linear_heat_capacity_J_C_m;

        filament_offset_c = std::min<float>(hotend_ambient_temp_diff_c * coef, 0);
    }

    // Calculate fan offset
    const float fan_offset_c = fan_offset_weight * sqrtf(params.print_fan_pwm / 255.0f) * hotend_ambient_temp_diff_c;

    // Calculate heatbreak offset
    const float heatbreak_offset_c = heatbreak_offset_weight * hotend_ambient_temp_diff_c;

    // Calculate heating offset
    const float heating_offset_c = heating_offset_weight * params.hotend_temp_readout_dt_c_s;

    const float final_temp_offset_c = compensation_factor * (fan_offset_c + heating_offset_c + heatbreak_offset_c + filament_offset_c);

    // Note: Exponential fadeout is then applied directly on the INDX head
    // Clamp to sane values, just in case
    return std::clamp(final_temp_offset_c, -80.f, 80.f);
}

void HotendTempCompensator::reset_state() {
    state_ = {};
}
} // namespace indx_hotend_temp_compensation

/// @file
#include "indx_hotend_thermal_model.hpp"

#include <cmath>
#include <algorithm>

#include <utils/math/ema.hpp>

namespace indx {

namespace {
    /// [°C/W]
    constexpr float ambient_thermal_resistance_C_W = 30.0f;

    /// [°C/W]
    constexpr float fan_thermal_resistance_C_W = 120.0f;

    // [J/°C], including silicone sock and average thermal capacity of filament in the nozzle
    constexpr float nozzle_heat_capacity_J_C = 1.75f;

} // namespace

bool HotendThermalModel::step(const StepParams &args) {
    const bool should_initialize = !is_initialized_;
    if (should_initialize) {
        dt_s_accum_ = 0;
        filament_feedrate_mm_s_ = args.extruder_feedrate_mm_s;

        print_fan_pwm_ema_ = args.print_fan_pwm;
        modelled_nozzle_temp_C_ = args.nozzle_temp_C;
        last_hotend_energy_consumed_uJ_ = args.hotend_energy_consumed_uJ;

        is_initialized_ = true;
    }

    // Print fan PWM EMA — runs every step for smooth convergence
    print_fan_pwm_ema_ = exponential_moving_average(print_fan_pwm_ema_, args.print_fan_pwm, args.dt_s, 0.7f);

    // Filament feedrate EMA — runs every step for smooth convergence
    filament_feedrate_mm_s_ = exponential_moving_average(filament_feedrate_mm_s_, args.extruder_feedrate_mm_s, args.dt_s, args.filament.heat_time_constant);

    // ACCUM INTERVAL
    // Do this less often, we want enough extruder and duty cycle delta to accumulate for the computation to be precise
    dt_s_accum_ += args.dt_s;
    if (dt_s_accum_ < accum_interval_s && !should_initialize) {
        return false;
    }

    // Update nozzle power from measured V × I integral
    {
        // Diff in uint32_t to handle overflows correctly
        const float total_power_W = uint32_t(args.hotend_energy_consumed_uJ - last_hotend_energy_consumed_uJ_) * 1e-6f / dt_s_accum_; // mW·ms / s → W
        modelled_nozzle_power_W_ = total_power_W * hotend_induction_efficiency;
    }

    // Update total_thermal_conductivity / loss estimates
    float total_thermal_conductivity_W_C;
    {
        // Heat loss power estimates [W]
        const float fan_thermal_conductivity_W_C = std::sqrt(print_fan_pwm_ema_ / 255.f) / fan_thermal_resistance_C_W;

        const float nozzle_ambient_temp_diff_C = args.nozzle_temp_C - args.chamber_temp_C;

        // Re-referencing ambient thermal conductivity to coil fixture temperature
        float ambient_thermal_conductivity_W_C = 1.f / ambient_thermal_resistance_C_W;
        if (nozzle_ambient_temp_diff_C > 50) {
            ambient_thermal_conductivity_W_C *= std::max<float>(args.nozzle_temp_C - args.board_temp_C, 0) / nozzle_ambient_temp_diff_C;
        }

        // Use heat_per_mm from compensation params (per-filament) scaled to J/(°C·mm)
        constexpr float heat_per_mm_to_capacity = 0.001f;
        const float filament_flow_thermal_conductivity_W_C = args.filament.heat_per_mm * heat_per_mm_to_capacity * filament_feedrate_mm_s_;

        total_thermal_conductivity_W_C = fan_thermal_conductivity_W_C + ambient_thermal_conductivity_W_C + filament_flow_thermal_conductivity_W_C;
    }

    // Update modelled nozzle temperature
    // Euler integration over the full accum interval — synchronous with power computation
    {
        // Modeled nozzle temperature: Euler integration of power balance
        // dT/dt = (P_in - P_out) / C, where P_out = conductivity × (T_nozzle - T_chamber)
        const float power_balance_W = modelled_nozzle_power_W_ - total_thermal_conductivity_W_C * (modelled_nozzle_temp_C_ - args.chamber_temp_C);
        modelled_nozzle_temp_C_ += (power_balance_W / nozzle_heat_capacity_J_C) * dt_s_accum_;
    }

    // !!! Update at the end of the accum interval code, not before
    dt_s_accum_ = 0;
    last_hotend_energy_consumed_uJ_ = args.hotend_energy_consumed_uJ;

    return true;
}

void HotendThermalModel::reset_state() {
    is_initialized_ = false;
}

} // namespace indx

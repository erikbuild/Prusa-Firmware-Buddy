/// @file
#pragma once

#include <cstdint>
#include <cmath>

#include "indx_filament_params.hpp"

namespace indx {

// More info in BFW-8702
class HotendThermalModel {

public:
    /// Fraction of the coil power delivered to nozzle
    static constexpr float hotend_induction_efficiency = 0.94f;

public:
    /// Input data for the thermal model, collected each cycle (~160ms)
    struct StepParams {
        /// Delta time since the last step [s]
        float dt_s;

        /// Integral of measured V × I power [mW × ms]
        /// Expected to wrap
        uint32_t hotend_energy_consumed_uJ;

        /// Measured, actual nozzle temperature [°C]
        float nozzle_temp_C;

        /// INDX head board NTC temperature [°C]
        float board_temp_C;

        /// How fast is the extruder feeding the filament [mm/s]
        float extruder_feedrate_mm_s;

        /// Print fan PWM [0–255]
        uint8_t print_fan_pwm;

        /// Chamber temperature [°C]
        float chamber_temp_C;

        /// Parameters of the inserted filament
        const FilamentParameters &filament;
    };

    /// Interval of the slower accumulation logic update
    static constexpr float accum_interval_s = 0.5f;

    /// @returns whether the model got updated (happens less often because of the accum interval)
    bool step(const StepParams &args);

    void reset_state();

    /// @returns nozzle temperature according to the model [°C]
    /// Gets updated only when step() returns true
    float modelled_nozzle_temp_C() const {
        return modelled_nozzle_temp_C_;
    }

private:
    // These should get appropriate values assigned once is_initialized_ gets set to true
    // Set to predictable invalid data to catch potential bugs

    /// Main output of the model - the expected nozzle temperature
    float modelled_nozzle_temp_C_ = NAN;

    /// Print fan PWM filtered through exponential mean average
    float print_fan_pwm_ema_ = NAN;

    /// How fast is the extruder feeding the filament [mm/s]
    /// Stored because we're applying EMA
    float filament_feedrate_mm_s_ = NAN;

    bool is_initialized_ : 1 = false;

private: // Slow updating variables updated in ACCUM_INTERVAL
    // =============================================================

    /// A part of the model (accum interval) gets updated in bigger intervals
    /// This variable accumulates the args.dt_s and if it reaches the threshold, runs the accum interval
    float dt_s_accum_ = NAN;

    /// Updated in the accum interval
    uint32_t last_hotend_energy_consumed_uJ_ = 0;

    /// Updated in the accum interval
    float modelled_nozzle_power_W_ = NAN;
};

} // namespace indx

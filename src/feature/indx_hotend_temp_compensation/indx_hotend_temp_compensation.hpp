/// @file
#pragma once

#include <cmath>
#include <cstdint>

struct FilamentTypeParameters;

// The thermometer on the head is basically not close enough to the melt zone
// and there is a significant difference between the measured temperature and what temp the filament is actually on.
// To compensate for this, we calculate a rudimentary filament model on the motherboard
// and send a compensation parameter over the modbus to the head.
// BFW-8630
namespace indx_hotend_temp_compensation {

/// See table in BFW-8630 for the computation algo.
struct FilamentParameters {
    /// Something to do with heat transfer.
    float heat_per_mm;

    /// Thermal time constant of filament, in seconds.
    /// Calculated as time to transfer (1-1/e) of heat into 2mm diameter cylinder. Inversely proportional to thermal diffusivity of the material.
    float heat_time_constant;

    /// Implemented in indx_filament_params.cpp
    static const FilamentParameters &for_filament(const FilamentTypeParameters &filament_parameters);
};

struct FilamentPrecomputedParameters {
    /// Constant parameter for linear filament offset formula if feedrate > threshold
    float const_coef;

    /// Linear parameter for linear filament offset formula if feedrate > threshold
    float linear_coef;

    /// Below this feedrate, offset is held constant at the threshold value
    float feedrate_threshold_mm_s;

    float heat_time_constant;

    static FilamentPrecomputedParameters compute(const FilamentParameters &params);
};

struct StepParams {
    /// Delta time since the last step, in seconds
    float dt_s;

    /// Temperature of the chamber (around the hotend), in °C
    float chamber_temperature_c;

    /// How fast is the extruder feeding the filament, in mm/s
    float extruder_feedrate_mm_s;

    /// Hotend temperature readout from the temp sensor, in °C
    /// !!! HAS TO BE UNCOMPENSATED
    float hotend_temp_readout_c;

    /// Derivative of the temp sensor readout, in °C/s
    /// !!! HAS TO BE UNCOMPENSATED
    float hotend_temp_readout_dt_c_s;

    /// PWM of the print fan, 0-255
    uint8_t print_fan_pwm;
};

/// Platform-independent compensator with pure inputs and outputs
/// Gets wired up to the moobo in buddy_indx_hotend_temp_compensation
/// This class is kept plain without dependencies for unittests
class HotendTempCompensator {

public:
    void set_filament_parameters(const FilamentParameters &set);
    void set_filament_parameters(const FilamentPrecomputedParameters &set);

    /// Steps once
    /// Needs to be called every step_dt_s
    float step(const StepParams &params);

    void reset_state();

private:
    FilamentPrecomputedParameters filament_;

    struct State {
        /// Feedrate with exponential fadeoff applied
        float feedrate_mm_s = 0;
    };
    State state_;
};

} // namespace indx_hotend_temp_compensation

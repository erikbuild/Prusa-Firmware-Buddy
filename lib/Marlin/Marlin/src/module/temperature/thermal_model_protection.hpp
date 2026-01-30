/// @file
#pragma once

#include <inc/MarlinConfig.h>

class ThermalModelProtection {

public:
    /**
     * @brief Detect discrepancy between expected heating based on model and actual heating
     *
     * PWM output is checked once per 1 second. Each time it is THERMAL_PROTECTION_MODEL_DISCREPANCY
     * over feed_forward value failed_cycles are incremented. Each time it is under it is decremented.
     * Once failed_cycles reaches over THERMAL_PROTECTION_MODEL_PERIOD, temperature error is announced.
     *
     * @param pid_output heater PWM output
     * @param feed_forward part of the heater PWM output not affected by temperature readings
     * @param e hotend index
     */
    void thermal_model_protection(const float &pid_output, const float &feed_forward, const uint8_t E_NAME);

    bool is_ok() const;

private:
    millis_t timer = 0;
    int_least8_t failed_cycles = 0;
};

/// @file
#pragma once

#include <core/millis_t.h>
#include <cstdint>
#include <module/temperature/temp_defines.hpp>

class ThermalRunaway {

public:
    /// Manages the thermal runaway protection
    /// Kills the printer if it detects a problem
    void step(const float &current, const float &target, const heater_ind_t heater_id, const uint16_t period_seconds, const uint16_t hysteresis_degc, bool reset);

private:
    enum State : uint8_t {
        TRInactive,
        TRFirstHeating,
        TRStable,
        TRRunaway,
    };

private:
    millis_t timer = 0;
    State state = TRInactive;
    float tr_target_temperature = 0.0;
};

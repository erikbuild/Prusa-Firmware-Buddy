/// @file
#pragma once

#include <core/millis_t.h>
#include <cstdint>
#include <module/temperature/temp_defines.hpp>

enum TRState : uint8_t {
    TRInactive,
    TRFirstHeating,
    TRStable,
    TRRunaway,
};

struct tr_state_machine_t {
    millis_t timer = 0;
    TRState state = TRInactive;
};

void thermal_runaway_protection(tr_state_machine_t &state, const float &current, const float &target, const heater_ind_t heater_id, const uint16_t period_seconds, const uint16_t hysteresis_degc);

/// @file
#include "thermal_runaway.hpp"

#include <module/temperature.h>

void thermal_runaway_protection(tr_state_machine_t &sm, const float &current, const float &target, const heater_ind_t heater_id, const uint16_t period_seconds, const uint16_t hysteresis_degc, bool reset) {
#if HEATER_IDLE_HANDLER
    // If the heater idle timeout expires, restart
    if (reset) {
        sm.state = TRInactive;
        sm.tr_target_temperature = 0;
    } else
#endif
    {
        // If the target temperature changes, restart
        if (sm.tr_target_temperature != target) {
            sm.tr_target_temperature = target;
            sm.state = target > 0 ? TRFirstHeating : TRInactive;
        }
    }

    switch (sm.state) {
    // Inactive state waits for a target temperature to be set
    case TRInactive:
        break;

    // When first heating, wait for the temperature to be reached then go to Stable state
    case TRFirstHeating:
        if (current < sm.tr_target_temperature) {
            break;
        }
        sm.state = TRStable;

    // While the temperature is stable watch for a bad temperature
    case TRStable:
        if (current >= sm.tr_target_temperature - hysteresis_degc) {
            sm.timer = millis() + period_seconds * 1000UL;
            break;
        } else if (PENDING(millis(), sm.timer)) {
            break;
        }
        sm.state = TRRunaway;

    case TRRunaway:
        if (heater_id == H_BED) {
            thermalManager._temp_error(heater_id, PSTR(MSG_T_THERMAL_RUNAWAY), GET_TEXT(MSG_THERMAL_RUNAWAY_BED));
        } else {
            thermalManager._temp_error(heater_id, PSTR(MSG_T_THERMAL_RUNAWAY), GET_TEXT(MSG_THERMAL_RUNAWAY));
        }
    }
}

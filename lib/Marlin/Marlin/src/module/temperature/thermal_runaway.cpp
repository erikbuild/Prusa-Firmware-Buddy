/// @file
#include "thermal_runaway.hpp"

#include <module/temperature.h>

void thermal_runaway_protection(tr_state_machine_t &sm, const float &current, const float &target, const heater_ind_t heater_id, const uint16_t period_seconds, const uint16_t hysteresis_degc) {

    static float tr_target_temperature[HOTENDS + 1] = { 0.0 };

    /**
      SERIAL_ECHO_START();
      SERIAL_ECHOPGM("Thermal Thermal Runaway Running. Heater ID: ");
      if (heater_id == H_CHAMBER) SERIAL_ECHOPGM("chamber");
      if (heater_id < 0) SERIAL_ECHOPGM("bed"); else SERIAL_ECHO(heater_id);
      SERIAL_ECHOPAIR(" ;  State:", sm.state, " ;  Timer:", sm.timer, " ;  Temperature:", current, " ;  Target Temp:", target);
      if (heater_id >= 0)
        SERIAL_ECHOPAIR(" ;  Idle Timeout:", hotend_idle[heater_id].timed_out);
      else
        SERIAL_ECHOPAIR(" ;  Idle Timeout:", bed_idle.timed_out);
      SERIAL_EOL();
    //*/

    if (heater_id >= HOTENDS) {
        bsod("Not implemented"); // thermal protection is implemened only for BED+HOTENDS, not Heatbreaks etc
    }

    const int heater_index = heater_id >= 0 ? heater_id : HOTENDS;

#if HEATER_IDLE_HANDLER
    // If the heater idle timeout expires, restart
    if ((heater_id >= 0 && thermalManager.hotend_idle[heater_id].timed_out)
    #if HAS_HEATED_BED
        || (heater_id < 0 && thermalManager.bed_idle.timed_out)
    #endif
    ) {
        sm.state = TRInactive;
        tr_target_temperature[heater_index] = 0;
    } else
#endif
    {
        // If the target temperature changes, restart
        if (tr_target_temperature[heater_index] != target) {
            tr_target_temperature[heater_index] = target;
            sm.state = target > 0 ? TRFirstHeating : TRInactive;
        }
    }

    switch (sm.state) {
    // Inactive state waits for a target temperature to be set
    case TRInactive:
        break;

    // When first heating, wait for the temperature to be reached then go to Stable state
    case TRFirstHeating:
        if (current < tr_target_temperature[heater_index]) {
            break;
        }
        sm.state = TRStable;

    // While the temperature is stable watch for a bad temperature
    case TRStable:
        if (current >= tr_target_temperature[heater_index] - hysteresis_degc) {
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

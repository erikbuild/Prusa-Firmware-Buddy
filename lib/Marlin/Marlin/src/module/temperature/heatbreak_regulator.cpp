///@file
#include "heatbreak_regulator.hpp"

#include <inc/MarlinConfig.h>
#include <module/temperature/temp_defines.hpp>
#include <module/temperature.h>

// TODO: Somehow move this to CMAKE?
#if ENABLED(PIDTEMPHEATBREAK)

float HeatbreakRegulator::step() {
    static_assert(HOTENDS <= 1, "Not implemented for more hotends.");

    // TODO: Get rid of these links
    auto &temp_heatbreak = thermalManager.temp_heatbreak;
    auto &temp_hotend = thermalManager.temp_hotend;

    #if DISABLED(PID_OPENLOOP)

    static PID_t work_pid { 0 };
    static float temp_iState = 0, temp_dState = 0;
    static bool pid_reset = true;
    static int fan_kick_counter = 0;
    float pid_output = 0;
    const float pid_error = temp_heatbreak[0].celsius - temp_heatbreak[0].target;

    if (pid_reset) {
        temp_iState = 0.0;
        work_pid.Kd = 0.0;
        temp_dState = pid_error;
        pid_reset = false;
    }

    work_pid.Kp = temp_heatbreak[0].pid.Kp * pid_error;
    work_pid.Kd = work_pid.Kd + HEATBREAK_PID_K2 * (temp_heatbreak[0].pid.Kd * (pid_error - temp_dState) - work_pid.Kd);

    pid_output = work_pid.Kp + work_pid.Kd;

    // Sum error only if it has effect on output value
    if (!((((pid_output + work_pid.Ki) < 0) && (pid_error < 0))
            || (((pid_output + work_pid.Ki) > MAX_HEATBREAK_POWER) && (pid_error > 0)))) {
        temp_iState += pid_error;
    }
    work_pid.Ki = temp_heatbreak[0].pid.Ki * temp_iState;

    pid_output += work_pid.Ki;
    temp_dState = pid_error;

    if (temp_hotend[0].celsius > HEATBREAK_FAN_ALWAYS_ON_NOZZLE_TEMPERATURE) {
        pid_output = constrain(pid_output, MIN_STOP_HEATBREAK_POWER, MAX_HEATBREAK_POWER);
        if (work_pid.Ki < MIN_STOP_HEATBREAK_POWER) {
            temp_iState = MIN_STOP_HEATBREAK_POWER / temp_heatbreak[0].pid.Ki;
        }
    } else {
        pid_output = constrain(pid_output, 0, MAX_HEATBREAK_POWER);
    }

    if (pid_output < MIN_STOP_HEATBREAK_POWER) {
        pid_output = 0;
        fan_kick_counter = 0;
    } else {
        if (fan_kick_counter) {
            if (-1 == HEATBREAK_FAN_KICK_CYCLES) {
                fan_kick_counter = 1;
            } else {
                if (pid_output < (MIN_START_HEATBREAK_POWER)) {
                    ++fan_kick_counter;
                } else {
                    fan_kick_counter = 1;
                }
                if (fan_kick_counter > HEATBREAK_FAN_KICK_CYCLES) {
                    fan_kick_counter = 0;
                }
            }
        } else {
            if (pid_output < (MIN_START_HEATBREAK_POWER)) {
                pid_output = MIN_START_HEATBREAK_POWER;
                ++fan_kick_counter;
            }
        }
    }

    #else // PID_OPENLOOP

    const float pid_output = constrain(temp_heatbreak[0].target, 0, MAX_BED_POWER);

    #endif // PID_OPENLOOP

    #if ENABLED(PID_HEATBREAK_DEBUG)
    {
        SERIAL_ECHO_START();
        SERIAL_ECHOLNPAIR(
            " PID_HEATBREAK_DEBUG : Input ", temp_heatbreak[0].celsius, " Output ", pid_output, " fan_kick_counter ", fan_kick_counter, " temp_hotend[0].celsius ", temp_hotend[0].celsius,
        #if DISABLED(PID_OPENLOOP)
            MSG_PID_DEBUG_PTERM, work_pid.Kp,
            MSG_PID_DEBUG_ITERM, work_pid.Ki,
            MSG_PID_DEBUG_DTERM, work_pid.Kd,
        #endif
        );
    }
    #endif

    return pid_output;
}

#endif

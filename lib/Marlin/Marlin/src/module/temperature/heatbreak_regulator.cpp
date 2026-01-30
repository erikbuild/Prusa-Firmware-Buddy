///@file
#include "heatbreak_regulator.hpp"

#include <inc/MarlinConfig.h>
#include <module/temperature/temp_defines.hpp>
#include <module/temperature.h>

// TODO: Somehow move this to CMAKE?
#if ENABLED(PIDTEMPHEATBREAK)

float HeatbreakRegulator::step(const Args &args) {
    static_assert(HOTENDS <= 1, "Not implemented for more hotends.");

    // TODO: Rethink where the PID config is stored
    const auto &pid = thermalManager.temp_heatbreak[0].pid;

    #if DISABLED(PID_OPENLOOP)
    float pid_output = 0;
    const float pid_error = args.current_temp - args.target_temp;

    if (pid_reset) {
        temp_iState = 0.0;
        work_pid.Kd = 0.0;
        temp_dState = pid_error;
        pid_reset = false;
    }

    work_pid.Kp = pid.Kp * pid_error;
    work_pid.Kd = work_pid.Kd + HEATBREAK_PID_K2 * (pid.Kd * (pid_error - temp_dState) - work_pid.Kd);

    pid_output = work_pid.Kp + work_pid.Kd;

    // Sum error only if it has effect on output value
    if (!((((pid_output + work_pid.Ki) < 0) && (pid_error < 0))
            || (((pid_output + work_pid.Ki) > MAX_HEATBREAK_POWER) && (pid_error > 0)))) {
        temp_iState += pid_error;
    }
    work_pid.Ki = pid.Ki * temp_iState;

    pid_output += work_pid.Ki;
    temp_dState = pid_error;

    if (args.current_hotend_temp > HEATBREAK_FAN_ALWAYS_ON_NOZZLE_TEMPERATURE) {
        pid_output = constrain(pid_output, MIN_STOP_HEATBREAK_POWER, MAX_HEATBREAK_POWER);
        if (work_pid.Ki < MIN_STOP_HEATBREAK_POWER) {
            temp_iState = MIN_STOP_HEATBREAK_POWER / pid.Ki;
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

    const float pid_output = constrain(args.target_temp, 0, MAX_HEATBREAK_POWER);

    #endif // PID_OPENLOOP

    #if ENABLED(PID_HEATBREAK_DEBUG)
    {
        SERIAL_ECHO_START();
        SERIAL_ECHOLNPAIR(
            " PID_HEATBREAK_DEBUG : Input ", args.current_temp, " Output ", pid_output, " fan_kick_counter ", fan_kick_counter, " current_hotend_temp ", args.current_hotend_temp
        #if DISABLED(PID_OPENLOOP)
            ,
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

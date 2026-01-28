/// @file
#include "standard_hotend_regulator.hpp"

#include <module/temperature.h>
#include <module/temperature/steady_state_hotend.hpp>
#include <module/stepper.h>

static_assert(ENABLED(PIDTEMP), "Not supported anymore");
static_assert(DISABLED(PID_OPENLOOP), "Not supported anymore");

static constexpr float sample_frequency = TEMP_TIMER_FREQUENCY / MIN_ADC_ISR_LOOPS / OVERSAMPLENR;

float StandardHotendRegulator::get_pid_output_hotend(const uint8_t E_NAME) {
    // TODO: Get rid of these dependencies on thermalManager
    auto &temp_hotend = thermalManager.temp_hotend;
    auto &fan_speed = thermalManager.fan_speed;
    auto &hotend_idle = thermalManager.hotend_idle;
#if ENABLED(PID_EXTRUSION_SCALING)
    auto &last_e_position = thermalManager.last_e_position;
    const auto extrusion_scaling_enabled = thermalManager.extrusion_scaling_enabled;
#endif

    const uint8_t ee = HOTEND_INDEX;

    static hotend_pid_t work_pid[HOTENDS];
    static float temp_iState[HOTENDS] = { 0 },
                 temp_dState[HOTENDS] = { 0 };
    static bool pid_reset[HOTENDS] = { false };
    const float pid_error = temp_hotend[ee].target - temp_hotend[ee].celsius;

    float pid_output;
#if ALL(STEADY_STATE_HOTEND, PID_DEBUG)
    float feed_forward_debug = -1.0f;
#endif

    if (temp_hotend[ee].target == 0
        || pid_error < -(PID_FUNCTIONAL_RANGE)
#if HEATER_IDLE_HANDLER
        || hotend_idle[ee].timed_out
#endif
    ) {
        pid_output = 0;
        pid_reset[ee] = true;
    } else if (pid_error > PID_FUNCTIONAL_RANGE) {
        pid_output = BANG_MAX;
        pid_reset[ee] = true;
    } else {
        if (pid_reset[ee]) {
            temp_iState[ee] = 0.0;
            work_pid[ee].Kd = 0.0;
            temp_dState[ee] = pid_error;
            pid_reset[ee] = false;
        }
#if FAN_COUNT > 0
        work_pid[ee].Kd = work_pid[ee].Kd + PID_K2 * (PID_PARAM(Kd, ee) * (pid_error - temp_dState[ee]) - work_pid[ee].Kd);
        work_pid[ee].Kp = PID_PARAM(Kp, ee) * pid_error;
        pid_output = work_pid[ee].Kp + float(MIN_POWER);

    #if ENABLED(STEADY_STATE_HOTEND)
        static constexpr float pid_max_inv = 1.0f / PID_MAX;
        const float feed_forward = steady_state_hotend(temp_hotend[ee].target, fan_speed[0] * pid_max_inv);
        #if ENABLED(PID_DEBUG)
        feed_forward_debug = feed_forward;
        #endif
        pid_output += feed_forward;
    #endif
#endif

#if ENABLED(PID_EXTRUSION_SCALING)
    #if HOTENDS == 1
        constexpr bool this_hotend = true;
    #else
        const bool this_hotend = (ee == active_extruder);
    #endif
        work_pid[ee].Kc = 0;
        if (this_hotend) {
            constexpr float distance_to_volume = std::numbers::pi_v<float> * std::pow(DEFAULT_NOMINAL_FILAMENT_DIA / 2, 2.f);
            constexpr float distance_to_volume_per_second = distance_to_volume * sample_frequency;
            uint32_t e_position = stepper.position(E_AXIS);
            const int32_t e_pos_diff = e_position - last_e_position;
            last_e_position = e_position;

            work_pid[ee].Kc = e_pos_diff * planner.mm_per_step[E_AXIS] * distance_to_volume_per_second * (temp_hotend[ee].celsius - ambient_temp) * PID_PARAM(Kc, ee);
            if (extrusion_scaling_enabled) {
                pid_output += work_pid[ee].Kc;
            }
        }
#endif // PID_EXTRUSION_SCALING

        // Sum error only if it has effect on output value before D term is applied
        if (!((((pid_output + work_pid[ee].Ki) < 0) && (pid_error < 0))
                || (((pid_output + work_pid[ee].Ki) > PID_MAX) && (pid_error > 0)))) {
            temp_iState[ee] += pid_error;
        }
        work_pid[ee].Ki = PID_PARAM(Ki, ee) * temp_iState[ee];
        pid_output += work_pid[ee].Ki + work_pid[ee].Kd;

        LIMIT(pid_output, 0, PID_MAX);
    }
    temp_dState[ee] = pid_error;

#if ENABLED(PID_DEBUG)
    if (ee == active_extruder) {
        SERIAL_ECHO_START();
        SERIAL_ECHOPAIR(
            MSG_PID_DEBUG, ee,
            MSG_PID_DEBUG_INPUT, temp_hotend[ee].celsius,
            MSG_PID_DEBUG_OUTPUT, pid_output);
        SERIAL_ECHOPAIR(
    #if ENABLED(STEADY_STATE_HOTEND)
            " fTerm ", feed_forward_debug,
    #endif
            MSG_PID_DEBUG_PTERM, work_pid[ee].Kp,
            MSG_PID_DEBUG_ITERM, work_pid[ee].Ki,
            MSG_PID_DEBUG_DTERM, work_pid[ee].Kd
    #if ENABLED(PID_EXTRUSION_SCALING)
            ,
            MSG_PID_DEBUG_CTERM, work_pid[ee].Kc
    #endif
        );
        SERIAL_EOL();
    }
#endif // PID_DEBUG
    return pid_output;
}

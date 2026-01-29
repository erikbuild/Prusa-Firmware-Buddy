/// @file
#include "standard_hotend_regulator.hpp"

#include <module/temperature.h>
#include <module/temperature/steady_state_hotend.hpp>
#include <module/stepper.h>

static_assert(ENABLED(PIDTEMP), "Not supported anymore");
static_assert(DISABLED(PID_OPENLOOP), "Not supported anymore");

static constexpr float sample_frequency = TEMP_TIMER_FREQUENCY / MIN_ADC_ISR_LOOPS / OVERSAMPLENR;

HotendRegulatorResult StandardHotendRegulator::get_pid_output_hotend(const HotendRegulatorArgs &args) {
    // TODO: Get rid of these dependencies on thermalManager
    auto &temp_hotend = thermalManager.temp_hotend;
    auto &hotend_idle = thermalManager.hotend_idle;
#if ENABLED(PID_EXTRUSION_SCALING)
    auto &last_e_position = thermalManager.last_e_position;
    const auto extrusion_scaling_enabled = thermalManager.extrusion_scaling_enabled;
#endif

    const uint8_t ee = args.hotend_index;

    const float pid_error = temp_hotend[ee].target - temp_hotend[ee].celsius;

    float pid_output;
    float feed_forward = 0;

    if (temp_hotend[ee].target == 0
        || pid_error < -(PID_FUNCTIONAL_RANGE)
#if HEATER_IDLE_HANDLER
        || hotend_idle[ee].timed_out
#endif
    ) {
        pid_output = 0;
        pid_reset = true;
    } else if (pid_error > PID_FUNCTIONAL_RANGE) {
        pid_output = BANG_MAX;
        pid_reset = true;
    } else {
        if (pid_reset) {
            temp_iState = 0.0;
            work_pid.Kd = 0.0;
            temp_dState = pid_error;
            pid_reset = false;
        }
#if FAN_COUNT > 0
        work_pid.Kd = work_pid.Kd + PID_K2 * (PID_PARAM(Kd, ee) * (pid_error - temp_dState) - work_pid.Kd);
        work_pid.Kp = PID_PARAM(Kp, ee) * pid_error;
        pid_output = work_pid.Kp + float(MIN_POWER);

    #if ENABLED(STEADY_STATE_HOTEND)
        static constexpr float pid_max_inv = 1.0f / PID_MAX;
        feed_forward = steady_state_hotend(temp_hotend[ee].target, args.fan_speed * pid_max_inv);
        pid_output += feed_forward;
    #endif
#endif

#if ENABLED(PID_EXTRUSION_SCALING)
    #if HOTENDS == 1
        constexpr bool this_hotend = true;
    #else
        const bool this_hotend = (ee == active_extruder);
    #endif
        work_pid.Kc = 0;
        if (this_hotend) {
            constexpr float distance_to_volume = std::numbers::pi_v<float> * std::pow(DEFAULT_NOMINAL_FILAMENT_DIA / 2, 2.f);
            constexpr float distance_to_volume_per_second = distance_to_volume * sample_frequency;
            uint32_t e_position = stepper.position(E_AXIS);
            const int32_t e_pos_diff = e_position - last_e_position;
            last_e_position = e_position;

            work_pid.Kc = e_pos_diff * planner.mm_per_step[E_AXIS] * distance_to_volume_per_second * (temp_hotend[ee].celsius - ambient_temp) * PID_PARAM(Kc, ee);
            if (extrusion_scaling_enabled) {
                pid_output += work_pid.Kc;
            }
        }
#endif // PID_EXTRUSION_SCALING

        // Sum error only if it has effect on output value before D term is applied
        if (!((((pid_output + work_pid.Ki) < 0) && (pid_error < 0))
                || (((pid_output + work_pid.Ki) > PID_MAX) && (pid_error > 0)))) {
            temp_iState += pid_error;
        }
        work_pid.Ki = PID_PARAM(Ki, ee) * temp_iState;
        pid_output += work_pid.Ki + work_pid.Kd;

        LIMIT(pid_output, 0, PID_MAX);
    }
    temp_dState = pid_error;

#if ENABLED(PID_DEBUG)
    if (ee == active_extruder) {
        SERIAL_ECHO_START();
        SERIAL_ECHOPAIR(
            MSG_PID_DEBUG, ee,
            MSG_PID_DEBUG_INPUT, temp_hotend[ee].celsius,
            MSG_PID_DEBUG_OUTPUT, pid_output);
        SERIAL_ECHOPAIR(
    #if ENABLED(STEADY_STATE_HOTEND)
            " fTerm ", feed_forward,
    #endif
            MSG_PID_DEBUG_PTERM, work_pid.Kp,
            MSG_PID_DEBUG_ITERM, work_pid.Ki,
            MSG_PID_DEBUG_DTERM, work_pid.Kd
    #if ENABLED(PID_EXTRUSION_SCALING)
            ,
            MSG_PID_DEBUG_CTERM, work_pid.Kc
    #endif
        );
        SERIAL_EOL();
    }
#endif // PID_DEBUG

    return HotendRegulatorResult {
        .pid_output = pid_output,
        .feed_forward = feed_forward,
    };
}

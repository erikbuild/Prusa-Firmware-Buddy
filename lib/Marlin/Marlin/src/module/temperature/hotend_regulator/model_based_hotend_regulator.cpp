/// @file
#include "model_based_hotend_regulator.hpp"

#include <module/temperature.h>
#include <module/temperature/steady_state_hotend.hpp>
#include <module/stepper.h>

static constexpr float sample_frequency = TEMP_TIMER_FREQUENCY / MIN_ADC_ISR_LOOPS / OVERSAMPLENR;

static_assert(ENABLED(PIDTEMP), "Not supported anymore");
static_assert(DISABLED(PID_OPENLOOP), "Not supported anymore");

//! @brief Get model output hotend
//!
//! @param last_target Target temperature for this cycle
//! (Can not be measured due to transport delay)
//! @param expected Expected measurable hotend temperature in this cycle
//! @param E_NAME hotend index

float ModelBasedHotendRegulator::get_model_output_hotend(float &last_target, float &expected, const uint8_t E_NAME) {
    // TODO: Get rid of these dependencies on thermalManager
    auto &temp_hotend = thermalManager.temp_hotend;
    auto &fan_speed = thermalManager.fan_speed;

    const uint8_t ee = HOTEND_INDEX;

    enum class Ramp : uint_least8_t {
        Up,
        Down,
        None,
    };

    constexpr float epsilon = 0.01f;
    constexpr float transport_delay_seconds = 5.60f;
    constexpr int transport_delay_cycles = static_cast<int>(transport_delay_seconds * sample_frequency);
    constexpr float transport_delay_cycles_inv = 1.0f / transport_delay_cycles;
    constexpr float deg_per_second = 3.58f; //!< temperature rise at full power at zero cooling loses
    constexpr float deg_per_cycle = deg_per_second / sample_frequency;
    constexpr float pid_max_inv = 1.0f / PID_MAX;

    float hotend_pwm = 0;

    static int delay = transport_delay_cycles;
    static Ramp state = Ramp::None;

    if (temp_hotend[ee].target > (last_target + epsilon)) {
        if (state != Ramp::Up) {
            delay = transport_delay_cycles;
            expected = last_target;
            state = Ramp::Up;
        }
        //! Target for less than full power, so regulator can catch
        //! with generated temperature curve at minimum voltage
        //! (rated - 5%) = 22.8 V and maximum heater resistance
        //! of 15.1 Ohm. P = 22.8/15.1*22.8 = 34.43 W
        //! = 86% P(rated)
        constexpr float target_heater_pwm = PID_MAX * 0.8607f;
        const float temp_diff = deg_per_cycle * pid_max_inv
            * (target_heater_pwm - steady_state_hotend(last_target, fan_speed[0] * pid_max_inv));
        last_target += temp_diff;
        if (delay > 1) {
            --delay;
        }
        expected += temp_diff / delay;
        if (last_target > temp_hotend[ee].target) {
            last_target = temp_hotend[ee].target;
        }
        hotend_pwm = target_heater_pwm;
    } else if (temp_hotend[ee].target < (last_target - epsilon)) {
        if (state != Ramp::Down) {
            delay = transport_delay_cycles;
            expected = last_target;
            state = Ramp::Down;
        }
        const float temp_diff = deg_per_cycle * pid_max_inv
            * steady_state_hotend(last_target, fan_speed[0] * pid_max_inv);
        last_target -= temp_diff;
        if (delay > 1) {
            --delay;
        }
        expected -= temp_diff / delay;
        if (last_target < temp_hotend[ee].target) {
            last_target = temp_hotend[ee].target;
        }
        hotend_pwm = 0;
    } else {
        state = Ramp::None;
        last_target = temp_hotend[ee].target;
        const float remaining = last_target - expected;
        if (expected > (last_target + epsilon)) {
            float diff = remaining * transport_delay_cycles_inv;
            if (abs(diff) < epsilon) {
                diff = -epsilon;
            }
            expected += diff;
        } else if (expected < (last_target - epsilon)) {
            float diff = remaining * transport_delay_cycles_inv;
            if (abs(diff) < epsilon) {
                diff = epsilon;
            }
            expected += diff;
        } else {
            expected = last_target;
        }
        hotend_pwm = steady_state_hotend(last_target, fan_speed[0] * pid_max_inv);
    }
    return hotend_pwm;
}

float ModelBasedHotendRegulator::get_pid_output_hotend(
#if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
    float &feed_forward,
#endif
    const uint8_t E_NAME) {

    // TODO: Get rid of these dependencies on thermalManager
    auto &temp_hotend = thermalManager.temp_hotend;
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
    static float target_temp = .0;
    static float expected_temp = .0;

    float pid_output;
#if ENABLED(PID_DEBUG)
    float feed_forward_debug = -1.0f;
#endif

    if (temp_hotend[ee].target == 0
#if HEATER_IDLE_HANDLER
        || hotend_idle[ee].timed_out
#endif
    ) {
        pid_output = 0;
        pid_reset[ee] = true;
    } else {
        if (pid_reset[ee]) {
            temp_iState[ee] = 0.0;
            work_pid[ee].Kd = 0.0;
            target_temp = temp_hotend[ee].celsius;
            expected_temp = temp_hotend[ee].celsius;
            pid_reset[ee] = false;
        }
#if DISABLED(MODEL_DETECT_STUCK_THERMISTOR)
        const float
#endif
            feed_forward
            = get_model_output_hotend(target_temp, expected_temp, ee);
#if ENABLED(PID_DEBUG)
        feed_forward_debug = feed_forward;
#endif
        const float pid_error = expected_temp - temp_hotend[ee].celsius;
        work_pid[ee].Kd = work_pid[ee].Kd + PID_K2 * (PID_PARAM(Kd, ee) * (pid_error - temp_dState[ee]) - work_pid[ee].Kd);
        work_pid[ee].Kp = PID_PARAM(Kp, ee) * pid_error;

        pid_output = feed_forward + work_pid[ee].Kp + work_pid[ee].Kd + float(MIN_POWER);

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

            work_pid[ee].Kc = e_pos_diff * planner.mm_per_step[E_AXIS] * distance_to_volume_per_second * (temp_hotend[ee].target - ambient_temp) * PID_PARAM(Kc, ee);
            if (extrusion_scaling_enabled) {
                pid_output += work_pid[ee].Kc;
    #if ENABLED(MODEL_DETECT_STUCK_THERMISTOR)
                feed_forward += work_pid[ee].Kc;
    #endif
            }
        }
#endif // PID_EXTRUSION_SCALING

        // Sum error only if it has effect on output value
        if (!((((pid_output + work_pid[ee].Ki) < 0) && (pid_error < 0))
                || (((pid_output + work_pid[ee].Ki) > PID_MAX) && (pid_error > 0)))) {
            temp_iState[ee] += pid_error;
        }
        work_pid[ee].Ki = PID_PARAM(Ki, ee) * temp_iState[ee];
        pid_output += work_pid[ee].Ki;

        temp_dState[ee] = pid_error;
        LIMIT(pid_output, 0, PID_MAX);
    }

#if ENABLED(PID_DEBUG)
    if (ee == active_extruder) {
        SERIAL_ECHO_START();
        SERIAL_ECHOPAIR(
            MSG_PID_DEBUG, ee,
            MSG_PID_DEBUG_INPUT, temp_hotend[ee].celsius,
            MSG_PID_DEBUG_OUTPUT, pid_output);
        SERIAL_ECHOPAIR(
            " target ", expected_temp,
            " fTerm ", feed_forward_debug,
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

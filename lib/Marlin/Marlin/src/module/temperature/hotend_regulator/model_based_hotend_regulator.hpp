/// @file
#pragma once

#include <cstdint>
#include <inc/MarlinConfig.h>
#include <module/temperature/temp_defines.hpp>
#include <module/thermistor/thermistors.h>
#include <module/temperature/hotend_regulator/hotend_regulator.hpp>

class ModelBasedHotendRegulator {

public:
    HotendRegulatorResult get_pid_output_hotend(const HotendRegulatorArgs &args);

private:
    float get_model_output_hotend(const HotendRegulatorArgs &args);

    enum class Ramp : uint_least8_t {
        Up,
        Down,
        None,
    };

    static constexpr float sample_frequency = TEMP_TIMER_FREQUENCY / MIN_ADC_ISR_LOOPS / OVERSAMPLENR;

    static constexpr float epsilon = 0.01f;
    static constexpr float transport_delay_seconds = 5.60f;
    static constexpr int transport_delay_cycles = static_cast<int>(transport_delay_seconds * sample_frequency);
    static constexpr float transport_delay_cycles_inv = 1.0f / transport_delay_cycles;
    static constexpr float deg_per_second = 3.58f; //!< temperature rise at full power at zero cooling loses
    static constexpr float deg_per_cycle = deg_per_second / sample_frequency;
    static constexpr float pid_max_inv = 1.0f / PID_MAX;

    int delay = transport_delay_cycles;
    Ramp state = Ramp::None;

    HotendPIDConfig work_pid;
    float temp_iState = 0;
    float temp_dState = 0;
    bool pid_reset = false;
    float target_temp = 0;
    float expected_temp = 0;
};

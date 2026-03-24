/// @file
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <xbuddy_extension/shared_enums.hpp>

namespace hal {

using DutyCycle = uint8_t;

/**
 * Initialize hardware abstraction layer module.
 * This must be called while still in privileged mode,
 * because it needs to setup interrupts.
 */
void init();

/**
 * Enter infinite loop.
 */
[[noreturn]] void panic();

/**
 * Step the HAL subsystem.
 * This blocks and must be called periodically.
 */
void step();

// Each peripheral gets its own namespace

namespace fan1 {
    void set_pwm(DutyCycle duty_cycle);
    uint32_t get_rpm();
} // namespace fan1

namespace fan2 {
    void set_pwm(DutyCycle duty_cycle);
    uint32_t get_rpm();
} // namespace fan2

namespace fan3 {
    void set_pwm(DutyCycle duty_cycle);
    uint32_t get_rpm();
} // namespace fan3

namespace w_led {
    void set_pwm(DutyCycle duty_cycle);
    /**
     * Frequency of the PWM cycle
     *
     * In Hz.
     *
     * 0 means "default" selected by us.
     */
    void set_frequency(uint16_t freq);
} // namespace w_led

namespace rgbw_led {
    void set_r_pwm(DutyCycle duty_cycle);
    void set_g_pwm(DutyCycle duty_cycle);
    void set_b_pwm(DutyCycle duty_cycle);
    void set_w_pwm(DutyCycle duty_cycle);
} // namespace rgbw_led

namespace temperature {
    uint32_t get_raw();
}

namespace filament_sensor {
    using State = xbuddy_extension::FilamentSensorState;

    /// Single GPIO sensor (PA5 on standard, PA9 on iX)
    State get_gpio();

    /// TMP1826 multi-tool sensors (PC14/EXT connector)
    State get_ext(uint8_t index);
} // namespace filament_sensor

} // namespace hal

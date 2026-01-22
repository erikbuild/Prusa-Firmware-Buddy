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

    State get();
} // namespace filament_sensor

namespace rs485 {

    /**
     * Start receiving messages.
     * Does not block.
     */
    void start_receiving();

    /**
     * Blocks until message is received.
     * Returned span is valid until next transmit()
     */
    std::span<std::byte> receive();

    /// Blocks until message is received or timeout occurs.
    /// Returned span is valid until next transmit()
    /// Returns empty span if timeout occurs.
    std::span<std::byte> receive_timeout(uint32_t timeout_ms);

    /**
     * Transmit message.
     * Does not block.
     * Supplied span must remain valid until next receive()
     */
    void transmit_and_then_start_receiving(std::span<std::byte>);

    /**
     * Clear bus errors if needed.
     */
    void housekeeping();

} // namespace rs485

} // namespace hal

/// @file
#pragma once

#include <array>
#include <cstdint>
#include <xbuddy_extension/shared_enums.hpp>

/// This file defines MODBUS register files, to be shared between master and slave.
/// Resist the temptation to make this type-safe in any way! This is only used for
/// memory layout and should consist of 16-bit values, arrays and structures of such.
/// To ensure proper synchronization, you must always read/write entire register files.

namespace xbuddy_extension::modbus {

/// MODBUS register file for reporting current status of xBuddyExtension to motherboard.
struct Status {
    static constexpr uint16_t address = 0x8000;

    std::array<uint16_t, fan_count> fan_rpm; /// RPM of the fan
    uint16_t temperature; /// decidegree Celsius (eg. 23.5°C = 235 in the register)
    uint16_t filament_sensor; /// FilamentSensorState
};

/// MODBUS register file for setting desired config of xBuddyExtension from motherboard.
struct Config {
    static constexpr uint16_t address = 0x9000;

    std::array<uint16_t, fan_count> fan_pwm; /// PWM of the fan (0-255)
    uint16_t w_led_pwm; /// white led strip intensity (0-255)
    uint16_t rgbw_led_r_pwm; /// RGBW led strip, red component (0-255)
    uint16_t rgbw_led_g_pwm; /// RGBW led strip, green component (0-255)
    uint16_t rgbw_led_b_pwm; /// RGBW led strip, blue component (0-255)
    uint16_t rgbw_led_w_pwm; /// RGBW led strip, white component (0-255)
    uint16_t usb_power; /// enable power for usb port (boolean)
    uint16_t mmu_power; /// enable power for MMU port (boolean)
    uint16_t mmu_nreset; /// MMU port inverted reset pin value (boolean)

    /// Frequency of the white led PWM.
    ///
    /// 0 = default left to discretion of the extension board.
    /// Is the frequency of the full cycle, in Hz.
    ///
    /// Can be used to implement a "strobe"
    ///
    /// Warning: PWM timer shared with some fans.
    uint16_t w_led_frequency;
};

} // namespace xbuddy_extension::modbus

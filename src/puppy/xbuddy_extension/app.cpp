/// @file
#include "app.hpp"

#include "hal.hpp"
#include "mmu.hpp"
#include "modbus.hpp"
#include "temperature.hpp"
#include <cstdlib>
#include <span>
#include <freertos/timing.hpp>
#include <xbuddy_extension/mmu_bridge.hpp>
#include <xbuddy_extension/modbus.hpp>

namespace {

void read_register_file_callback(xbuddy_extension::modbus::Status &status) {
    status.fan_rpm[0] = hal::fan1::get_rpm();
    status.fan_rpm[1] = hal::fan2::get_rpm();
    status.fan_rpm[2] = hal::fan3::get_rpm();
    // Note: Mainboard expects this in decidegree Celsius.
    status.temperature = 10 * temperature::raw_to_celsius(hal::temperature::get_raw());
    status.filament_sensor = hal::filament_sensor::get();
}

bool write_register_file_callback(const xbuddy_extension::modbus::Config &config) {
    hal::fan1::set_pwm(config.fan_pwm[0]);
    hal::fan2::set_pwm(config.fan_pwm[1]);
    hal::fan3::set_pwm(config.fan_pwm[2]);
    hal::w_led::set_pwm(config.w_led_pwm);
    hal::rgbw_led::set_r_pwm(config.rgbw_led_r_pwm);
    hal::rgbw_led::set_g_pwm(config.rgbw_led_g_pwm);
    hal::rgbw_led::set_b_pwm(config.rgbw_led_b_pwm);
    hal::rgbw_led::set_w_pwm(config.rgbw_led_w_pwm);
    hal::usb::power_pin_set(static_cast<bool>(config.usb_power));
    hal::mmu::power_pin_set(static_cast<bool>(config.mmu_power));
    hal::mmu::nreset_pin_set(static_cast<bool>(config.mmu_nreset));
    // Technically, this frequency is common also for some fans. But they seem to work fine.
    hal::w_led::set_frequency(config.w_led_frequency);
    return true;
}

// TODO decide how to handle weird indexing schizophrenia caused by PuppyBootstrap::get_modbus_address_for_dock()
constexpr uint16_t MY_MODBUS_ADDR = 0x1a + 7;
constexpr uint16_t MMU_MODBUS_ADDR = xbuddy_extension::mmu_bridge::modbusUnitNr;

using Status = modbus::Callbacks::Status;

/// Read a register from a struct mapped to Modbus address space.
template <class T>
Status read_register_file(uint16_t address, std::span<uint16_t> out) {
    static_assert(sizeof(T) % 2 == 0);
    static_assert(alignof(T) == 2);
    if (T::address == address && out.size() == sizeof(T) / 2) {
        read_register_file_callback(*reinterpret_cast<T *>(out.data()));
        return Status::Ok;
    } else {
        return Status::IllegalAddress;
    }
}

/// Write a register to a struct mapped to Modbus address space.
template <class T>
Status write_register_file(uint16_t address, std::span<const uint16_t> in) {
    static_assert(sizeof(T) % 2 == 0);
    static_assert(alignof(T) == 2);
    if (T::address == address && in.size() == sizeof(T) / 2) {
        return write_register_file_callback(*reinterpret_cast<const T *>(in.data())) ? Status::Ok : Status::SlaveDeviceFailure;
    } else {
        return Status::IllegalAddress;
    }
}

class Logic final : public modbus::Callbacks {
public:
    Status read_registers(uint8_t, const uint16_t address, std::span<uint16_t> out) final {
        return read_register_file<xbuddy_extension::modbus::Status>(address, out);
    }

    Status write_registers(uint8_t, const uint16_t address, std::span<const uint16_t> in) final {
        return write_register_file<xbuddy_extension::modbus::Config>(address, in);
    }
};

class Dispatch final : public modbus::Callbacks {
public:
    struct SubDevice {
        uint8_t id;
        modbus::Callbacks *callbacks;
    };

private:
    std::span<SubDevice> sub_devices;

    template <class F>
    Status with(uint8_t device, F f) {
        for (const auto &sub_device : sub_devices) {
            if (sub_device.id == device) {
                return f(*sub_device.callbacks);
            }
        }

        return Status::Ignore;
    }

    bool all_distinct() {
        for (size_t i = 0; i < sub_devices.size(); i++) {
            for (size_t j = i + 1; j < sub_devices.size(); j++) {
                if (sub_devices[i].id == sub_devices[j].id) {
                    return false;
                }
            }
        }
        return true;
    }

public:
    Dispatch(std::span<SubDevice> sub_devices)
        : sub_devices { sub_devices } {
        if (!all_distinct()) {
            abort();
        }
    }

    Status read_registers(uint8_t device, uint16_t address, std::span<uint16_t> out) {
        return with(device, [&](auto &cbacks) { return cbacks.read_registers(device, address, out); });
    }

    Status write_registers(uint8_t device, uint16_t address, std::span<const uint16_t> in) {
        return with(device, [&](auto &cbacks) { return cbacks.write_registers(device, address, in); });
    }
};

void ensure_silent_interval() {
    // MODBUS over serial line specification and implementation guide V1.02
    // 2.5.1.1 MODBUS Message RTU Framing
    // In RTU mode, message frames are separated by a silent interval
    // of at least 3.5 character times.
    //
    // We are using 230400 bauds which means silent time ~0.15ms
    // Tick resolution is 1ms, meaning we are waiting longer than necessary.
    // Implementing smaller delay could improve MODBUS throughput, but may
    // not be worth the increased MCU resources consumption.
    freertos::delay(1);
}

} // namespace

void app::run() {
    Logic logic;
    MMU mmu;

    std::array sub_devices = {
        Dispatch::SubDevice { MY_MODBUS_ADDR, &logic },
        Dispatch::SubDevice { MMU_MODBUS_ADDR, &mmu },
    };

    Dispatch modbus_callbacks { sub_devices };

    alignas(uint16_t) std::byte response_buffer[32]; // is enough for now
    hal::rs485::start_receiving();
    for (;;) {
        const auto request = hal::rs485::receive();
        const auto response = modbus::handle_transaction(modbus_callbacks, request, response_buffer);
        if (response.size()) {
            ensure_silent_interval();
            hal::rs485::transmit_and_then_start_receiving(response);
        } else {
            hal::rs485::start_receiving();
        }
    }
}

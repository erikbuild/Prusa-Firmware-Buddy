#pragma once

#include <span>
#include <cstddef>

namespace hal {

void init();
void set_status_led(bool set);

void __attribute__((noreturn)) panic();
void __attribute__((noreturn)) reset();

namespace memory {

    /// Memory address region dedicated to peripherals control
    extern const std::span<std::byte> peripheral_address_region;

}; // namespace memory

} // namespace hal

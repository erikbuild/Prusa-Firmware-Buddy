/// @file
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace hal::crc {

void init();

uint16_t compute_crc16_modbus(std::span<const std::byte>);

} // namespace hal::crc

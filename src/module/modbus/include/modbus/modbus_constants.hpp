#pragma once
#include <cstddef>
// Modbus function codes
namespace modbus::fc {

// Please, keep this sorted by numerical value.
constexpr std::byte read_coils { 0x01 };
constexpr std::byte read_holding_registers { 0x03 };
constexpr std::byte read_input_registers { 0x04 };
constexpr std::byte write_coil { 0x05 };
constexpr std::byte write_coils { 0x0F };
constexpr std::byte write_multiple_registers { 0x10 };

} // namespace modbus::fc

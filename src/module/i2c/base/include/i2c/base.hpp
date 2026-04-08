#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace i2c {
using Address = uint8_t;

template <typename HWImpl>
concept Device = requires(HWImpl impl, Address address, size_t offset, std::span<const std::byte> tx_buf, std::span<std::byte> rx_buf) {
    { impl.write_memory(address, offset, tx_buf) } -> std::same_as<bool>;
    { impl.read_memory(address, offset, rx_buf) } -> std::same_as<bool>;
    { impl.raw_transmit(address, offset, tx_buf) } -> std::same_as<bool>;
    { impl.raw_receive(address, offset, rx_buf) } -> std::same_as<bool>;
};
} // namespace i2c

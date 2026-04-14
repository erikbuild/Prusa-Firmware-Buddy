/// @file
#pragma once

#include <cstddef>
#include <span>

namespace spi {

template <typename T>
concept Device = requires(T t) {
    { t.transmit_receive(std::span<const std::byte> {}, std::span<std::byte> {}) } -> std::same_as<bool>;
};

} // namespace spi

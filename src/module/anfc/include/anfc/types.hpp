/// @file
#pragma once

#include <cstddef>
#include <cstdint>

namespace anfc {

/// Represents virtual ANFC device.
enum class Device : uint8_t {
    anfc0,
    anfc1,
};

/// How many virtual ANFC devices are supported by the firmware.
inline constexpr size_t device_count = 2;

} // namespace anfc

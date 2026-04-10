#pragma once

#include <cstdint>
#include <array>

namespace fifo_coder {

/**
 * FIFO coded stream
 *
 * Instead of passing data as raw Modbus registers in FIFO stream this defines messages
 * on top of FIFO stream data.
 *
 * A message looks like as follows:
 * - 1 byte message type (MessageType)
 * - static (per message type) payload
 *
 * Important notes:
 * - Messages do not cross FIFO transfer boundaries - each transfer can be decoded into
 *   message upon receival without additional buffering.
 * - Message type 0 means no message - padding. This allows to pad data with 0 if necessary.
 */

constexpr auto MODBUS_FIFO_LEN = 31;

enum class MessageType : uint8_t {
    no_data = 0, // Padding
    log = 1,
    loadcell = 2,
    accelerometer_fast = 4, ///< Multiple samples without timestamp
    accelerometer_sampling_rate = 5, /// Single floating point number with frequency in Hz
    // ...
};

using TimeStamp_us = uint32_t;

struct [[gnu::packed]] Header {
    MessageType type;
};

// Payload types
using AccelerometerXyzSample = uint32_t;
using LoadcellSample_t = uint32_t;

struct [[gnu::packed]] LoadcellRecord {
    TimeStamp_us timestamp;
    LoadcellSample_t loadcell_raw_value;
};

using LogData = std::array<char, 8>;
using AccelerometerFastData = std::array<AccelerometerXyzSample, 2>;

struct AccelerometerSamplingRate {
    float frequency;
};

// Payload to message type mapping using template specialization
template <typename T>
constexpr MessageType message_type() {
    return MessageType::no_data;
}

template <>
constexpr MessageType message_type<LogData>() {
    return MessageType::log;
}

template <>
constexpr MessageType message_type<LoadcellRecord>() {
    return MessageType::loadcell;
}

template <>
constexpr MessageType message_type<AccelerometerFastData>() {
    return MessageType::accelerometer_fast;
}

template <>
constexpr MessageType message_type<AccelerometerSamplingRate>() {
    return MessageType::accelerometer_sampling_rate;
}
} // namespace fifo_coder

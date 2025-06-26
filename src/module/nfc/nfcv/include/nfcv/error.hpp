#pragma once

#include <expected>
#include <cstdint>

namespace nfcv {
/// Common error type for all the implementations and interface
/// Feel free to extended the values
enum class Error : uint8_t {
    timeout,
    invalid_chip,
    bad_oscilator,
    buffer_overflow,
    invalid_crc,
    device_hard_framing_error,
    device_soft_framing_error,
    device_parity_error,
    device_crc_error,
    no_response,
    response_invalid_size,
    response_format_invalid,
    response_is_error,
    bad_request,
    not_implemented,
    other,
    unknown,
};

template <typename T>
using Result = std::expected<T, Error>;
} // namespace nfcv

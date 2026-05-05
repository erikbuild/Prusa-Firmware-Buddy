/// @file
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <variant>

namespace base64 {

/// @returns how many Base64 characters will binary data of binary_length encode into
inline constexpr size_t encoded_length(size_t binary_length) {
    // ceil(n/3) * 4 — includes '=' padding
    return ((binary_length + 2) / 3) * 4;
}

/// Stream binary data -> Base64 string encoder
class Base64Encoder {

public:
    /// Feeds one byte into the encoder
    /// @returns a newly encoded data fragment (can be empty)
    /// The returned data is valid till the next encode or finalize call
    [[nodiscard]] std::string_view encode(std::byte byte);

    /// Finalizes the encoding - ensures proper alignment, makes sure all bytes are flushed
    /// @returns a newly encoded data fragment (can be empty)
    /// The returned data is valid till the next encode or finalize call
    [[nodiscard]] std::string_view finalize();

    /// Resets the state
    void reset();

private:
    uint16_t accumulator_ = 0;
    uint8_t accumulated_bit_count_ = 0;

    /// Buffer for storing the result of the encode() and finalize()
    /// Worst case is "X==" - one data char and two padding chars
    std::array<char, 3> result_buffer_;
};

/// Stream Base64 string -> binary data decoder
class Base64Decoder {

public:
    enum class DecodeResult : uint8_t {
        /// The decode step did not produce a new output character.
        no_output,

        /// The decode step did produce a new character that has been stored to the @p output
        new_output,

        /// The provided input is invalid
        error,
    };

    /// Consumes a single character input for decoding
    /// @param out The decoder sometimes produces a single decoded character that gets stored in the reference
    /// @returns whether there was an error and whether a new output has been produced
    [[nodiscard]] DecodeResult decode(char character, std::byte &out);

    /// Finalizes the decoding
    /// Resets the state and returns false if there were any dangling data left
    [[nodiscard]] bool finalize();

    /// Resets the state
    void reset();

private:
    uint16_t accumulator_ = 0;
    uint8_t accumulated_bit_count_ = 0;
};

}; // namespace base64

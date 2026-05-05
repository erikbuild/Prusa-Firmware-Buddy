#include <base64/base64.hpp>

#include <algorithm>

namespace base64 {

namespace {

    /// How many raw data bits one encoded character holds
    constexpr uint8_t bits_per_encoded_char = 6;

    constexpr uint8_t bits_per_encoded_char_mask = (1 << bits_per_encoded_char) - 1;

    /// Encoding table for Base64
    constexpr std::array encode_table {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        '+', '/'
    };

    constexpr char decode_table_min_char = std::ranges::min(encode_table);
    constexpr char decode_table_max_char = std::ranges::max(encode_table);

    /// Size of the base64_decode table
    constexpr size_t decode_table_size = size_t(decode_table_max_char - decode_table_min_char + 1);

    constexpr uint8_t invalid_decoded_bits { 0xff };

    /// Decoding table for Base64.
    /// decode_table[character - decode_table_min_char] = 6-bit value of the characer, or 0xff if invalid
    constexpr auto decode_table = [] {
        std::array<uint8_t, decode_table_size> result;
        result.fill(invalid_decoded_bits);

        for (size_t i = 0; i < result.size(); i++) {
            const char character = char(i + decode_table_min_char);
            const auto char_it = std::ranges::find(encode_table, character);

            if (char_it != encode_table.end()) {
                result[i] = (char_it - encode_table.begin());
            }
        }

        return result;
    }();
} // namespace

std::string_view Base64Encoder::encode(std::byte byte) {
    accumulator_ = (accumulator_ << 8) | static_cast<uint16_t>(byte);
    accumulated_bit_count_ += 8;

    uint8_t result_size = 0;

    while (accumulated_bit_count_ >= bits_per_encoded_char) {
        // Decrement first, that will make the "accumulator_ >> accumulated_bit_count_" work nicely
        accumulated_bit_count_ -= bits_per_encoded_char;

        // Take the MSB of the accumulator - as Base64 commands
        result_buffer_[result_size++] = encode_table[(accumulator_ >> accumulated_bit_count_) & bits_per_encoded_char_mask];
    }

    return std::string_view { result_buffer_.data(), result_size };
}

std::string_view Base64Encoder::finalize() {
    const uint8_t remaining_bit_count = accumulated_bit_count_;

    if (remaining_bit_count == 0) {
        // Properly aligned to 3 bytes -> noop
        return {};
    }

    // Align the accumulator for emitting the trailing codepoint
    accumulator_ <<= bits_per_encoded_char - remaining_bit_count;

    result_buffer_[0] = encode_table[accumulator_ & bits_per_encoded_char_mask];
    result_buffer_[1] = '=';

    // Will possibly be cropped in the return statement
    result_buffer_[2] = '=';

    reset();

    // Base64 states that if we have two bits accumulated left, we pad with two "==", otherwise with one "="
    return std::string_view { result_buffer_.data(), (remaining_bit_count == 2) ? size_t { 3 } : size_t { 2 } };
}

void Base64Encoder::reset() {
    accumulated_bit_count_ = 0;
}

Base64Decoder::DecodeResult Base64Decoder::decode(char character, std::byte &out) {
    [[unlikely]] if (character == '=') {
        reset();
        return DecodeResult::no_output;
    }

    [[unlikely]] if (character < decode_table_min_char || character > decode_table_max_char) {
        return DecodeResult::error;
    }

    const uint8_t decoded_bits = decode_table[character - decode_table_min_char];
    [[unlikely]] if (decoded_bits == invalid_decoded_bits) {
        return DecodeResult::error;
    }

    accumulator_ = (accumulator_ << bits_per_encoded_char) | decoded_bits;
    accumulated_bit_count_ += bits_per_encoded_char;

    if (accumulated_bit_count_ < 8) {
        return DecodeResult::no_output;
    }

    // Decrement first, that will make the "accumulator_ >> accumulated_bit_count_" work nicely
    accumulated_bit_count_ -= 8;

    // Take the MSB of the accumulator - as Base64 commands
    out = static_cast<std::byte>(accumulator_ >> accumulated_bit_count_);
    return DecodeResult::new_output;
}

bool Base64Decoder::finalize() {
    const bool is_ok = (accumulated_bit_count_ == 0);

    reset();

    return is_ok;
}

void Base64Decoder::reset() {
    accumulated_bit_count_ = 0;
}

} // namespace base64

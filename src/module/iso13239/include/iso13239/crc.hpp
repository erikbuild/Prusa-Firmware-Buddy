#pragma once

#include <cstdint>
#include <span>

namespace iso13239 {

class CRC {
public:
    constexpr CRC()
        : curr_value(0xffff) {}
    constexpr void add_byte(uint8_t byte) {

        byte ^= (uint8_t)(curr_value & 0xFFU);
        byte ^= (byte << 4);

        curr_value = (curr_value >> 8) ^ (((uint16_t)byte) << 8) ^ (((uint16_t)byte) << 3) ^ (((uint16_t)byte) >> 4);
    }
    constexpr void add_bytes(const std::span<const uint8_t> &bytes) {
        for (const auto byte : bytes) {
            add_byte(byte);
        }
    }
    constexpr uint16_t get_result() {
        return ~curr_value;
    }

private:
    uint16_t curr_value;
};

} // namespace iso13239

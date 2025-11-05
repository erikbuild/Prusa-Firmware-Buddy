#pragma once

#include <array>
#include <utility>
#include <cstdint>

namespace adc {

template <uint8_t precision>
    requires(precision >= 8 && precision <= 16)
struct Raw {
    constexpr Raw() = default;
    explicit constexpr Raw(uint16_t raw_value)
        : raw_value(raw_value) {}

    static constexpr std::size_t value_mask = (1 << precision) - 1;

    constexpr Raw(const Raw<precision> &other) = default;
    constexpr Raw<precision> &operator=(const Raw<precision> &other) = default;
    constexpr Raw(Raw<precision> &&other) = default;
    constexpr Raw<precision> &operator=(Raw<precision> &&other) = default;

    constexpr uint16_t to_raw() const {
        return raw_value;
    }

    template <uint8_t new_precission>
    constexpr Raw<new_precission> convert_precision() const {
        if constexpr (new_precission > precision) {
            return Raw<new_precission> { static_cast<uint16_t>(raw_value << (new_precission - precision)) };
        } else {
            return Raw<new_precission> { static_cast<uint16_t>(raw_value >> (precision - new_precission)) };
        }
    }

    constexpr std::strong_ordering operator<=>(const Raw<precision> &other) const = default;

protected:
    uint16_t raw_value;
};

using Temperature = int16_t;

enum class Channel : uint8_t {
    board_temp,
    _cnt
};

namespace impl {
    alignas(uint32_t) extern std::array<Raw<16>, std::to_underlying(Channel::_cnt)> buffer;

    Raw<16> get_raw(Channel channel);
} // namespace impl

Temperature get_board_temperature();
} // namespace adc

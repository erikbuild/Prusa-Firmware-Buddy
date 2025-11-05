#include "adc.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace adc {
namespace {
    constexpr auto ntcg104lh104jdts_conversion_table = std::to_array<std::pair<Raw<16>, Temperature>>({
        { Raw<16> { 0 }, 700 },
        { Raw<16> { 0x4e00 }, 125 },
        { Raw<16> { 0x5680 }, 120 },
        { Raw<16> { 0x5fc0 }, 115 },
        { Raw<16> { 0x6980 }, 110 },
        { Raw<16> { 0x73c0 }, 105 },
        { Raw<16> { 0x7e40 }, 100 },
        { Raw<16> { 0x8940 }, 95 },
        { Raw<16> { 0x9440 }, 90 },
        { Raw<16> { 0x9f40 }, 85 },
        { Raw<16> { 0xaa00 }, 80 },
        { Raw<16> { 0xb480 }, 75 },
        { Raw<16> { 0xbe40 }, 70 },
        { Raw<16> { 0xc780 }, 65 },
        { Raw<16> { 0xd000 }, 60 },
        { Raw<16> { 0xd780 }, 55 },
        { Raw<16> { 0xde40 }, 50 },
        { Raw<16> { 0xe440 }, 45 },
        { Raw<16> { 0xe940 }, 40 },
        { Raw<16> { 0xed80 }, 35 },
        { Raw<16> { 0xf140 }, 30 },
        { Raw<16> { 0xf440 }, 25 },
        { Raw<16> { 0xf6c0 }, 20 },
        { Raw<16> { 0xf8c0 }, 15 },
        { Raw<16> { 0xfa40 }, 10 },
        { Raw<16> { 0xfbc0 }, 5 },
        { Raw<16> { 0xfcc0 }, 0 },
        { Raw<16> { 0xfd80 }, -5 },
        { Raw<16> { 0xfe00 }, -10 },
        { Raw<16> { 0xfe80 }, -15 },
        { Raw<16> { 0xfec0 }, -20 },
        { Raw<16> { 0xff00 }, -25 },
        { Raw<16> { 0xff40 }, -30 },
    });

    constexpr Temperature calc_board_temp(Raw<16> raw_value) {
        const auto it = std::ranges::lower_bound(ntcg104lh104jdts_conversion_table, raw_value, std::less<Raw<16>> {}, [](const auto &val) { return val.first; });
        if (it == std::end(ntcg104lh104jdts_conversion_table)) {
            return std::numeric_limits<int16_t>::min();
        }
        const auto prev = it - 1;
        const auto diff = prev->first.to_raw() - raw_value.to_raw();
        const auto value_range = it->first.to_raw() - prev->first.to_raw();
        const auto temp_range = it->second - prev->second;
        const auto temp_diff = diff * temp_range / value_range;

        return prev->second - temp_diff;
    }

    // Test range of easily computable temperatures
    static_assert(calc_board_temp(Raw<16> { 0xf440 }) == 25);
    static_assert(calc_board_temp(Raw<16> { 0xf4c0 }) == 24);
    static_assert(calc_board_temp(Raw<16> { 0xf540 }) == 23);
    static_assert(calc_board_temp(Raw<16> { 0xf5c0 }) == 22);
    static_assert(calc_board_temp(Raw<16> { 0xf640 }) == 21);
    static_assert(calc_board_temp(Raw<16> { 0xf6c0 }) == 20);
} // namespace

alignas(uint32_t) std::array<Raw<16>, std::to_underlying(Channel::_cnt)> impl::buffer;

Raw<16> impl::get_raw(Channel channel) {
    assert(std::to_underlying(channel) < impl::buffer.size());
    return impl::buffer[std::to_underlying(channel)];
}

Temperature get_board_temperature() {
    const auto raw_temp = impl::get_raw(Channel::board_temp);
    return calc_board_temp(raw_temp);
}

} // namespace adc

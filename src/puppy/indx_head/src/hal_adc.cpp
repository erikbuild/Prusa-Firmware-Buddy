#include "hal.hpp"

namespace hal::adc {
alignas(uint32_t) std::array<uint16_t, std::to_underlying(Channel::_cnt)> impl::buffer {};
} // namespace hal::adc

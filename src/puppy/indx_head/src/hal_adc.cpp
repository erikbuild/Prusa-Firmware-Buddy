#include "hal.hpp"

#include <fpm/fixed.hpp>
#include <stm32c0xx_hal.h>

#include <algorithm>
#include <limits>

namespace hal::adc {
alignas(uint32_t) std::array<uint16_t, std::to_underlying(Channel::_cnt)> impl::buffer {};

uint16_t get_raw(Channel channel) {
    assert(channel != Channel::_cnt);
    return impl::buffer[std::to_underlying(channel)];
}

int16_t get_mcu_temp() {
    // Copied from https://github.com/STMicroelectronics/STM32CubeC0/blob/220515eaa28ef8848d304e6d52400aa2bbd84661/Projects/NUCLEO-C092RC/Examples_LL/ADC/ADC_MultiChannelSingleConversion_Init/Src/main.c#L258
    // and adjusted by values from datasheet and reference manual.
    uint16_t vref = __LL_ADC_CALC_VREFANALOG_VOLTAGE(get_raw(Channel::vref_int), LL_ADC_RESOLUTION_12B);
    uint16_t *ts_cal1 = TEMPSENSOR_CAL1_ADDR;
    uint16_t ts_v_at_30 = __LL_ADC_CALC_DATA_TO_VOLTAGE(TEMPSENSOR_CAL_VREFANALOG, *ts_cal1, LL_ADC_RESOLUTION_12B);
    return __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(2'530, ts_v_at_30, TEMPSENSOR_CAL1_TEMP, vref, get_raw(Channel::mcu_temp), LL_ADC_RESOLUTION_12B);
}

// Raw ADC values are 12-bit (0-4095)
constexpr auto tt7_10kc3_11_conversion_table = std::to_array<std::pair<uint16_t, int16_t>>({
    { 0, 700 }, // Boundary (open circuit / error)
    { 310, 120 },
    { 352, 115 },
    { 400, 110 },
    { 454, 105 },
    { 516, 100 },
    { 587, 95 },
    { 668, 90 },
    { 760, 85 },
    { 863, 80 },
    { 980, 75 },
    { 1111, 70 },
    { 1256, 65 },
    { 1415, 60 },
    { 1589, 55 },
    { 1774, 50 },
    { 1970, 45 },
    { 2173, 40 },
    { 2380, 35 },
    { 2585, 30 },
    { 2786, 25 },
    { 2977, 20 },
    { 3154, 15 },
    { 3315, 10 },
    { 3459, 5 },
    { 3583, 0 },
    { 3689, -5 },
    { 3777, -10 },
    { 3850, -15 },
    { 3908, -20 },
    { 3954, -25 },
    { 3990, -30 },
    { 4018, -35 },
    { 4039, -40 },
});

int16_t get_board_temp() {
    const auto raw_value = get_raw(Channel::board_temp);
    const auto it = std::ranges::lower_bound(tt7_10kc3_11_conversion_table, raw_value, std::less<uint16_t> {}, [](const auto &val) { return val.first; });
    if (it == std::end(tt7_10kc3_11_conversion_table)) {
        return std::numeric_limits<int16_t>::min();
    }
    const auto prev = it - 1;
    const auto diff = static_cast<int32_t>(prev->first) - raw_value;
    const auto value_range = static_cast<int32_t>(it->first) - prev->first;
    const auto temp_range = static_cast<int32_t>(it->second) - prev->second;
    const auto temp_diff = diff * temp_range / value_range;

    return prev->second - temp_diff;
}

uint16_t get_input_voltage() {
    static constexpr uint32_t r1 = 10; // kOhm
    static constexpr uint32_t r2 = 1; // kOhm
    static constexpr fpm::fixed_24_8 vref_nominal { 3.3f };

    // Voltage at ADC pin
    const auto v_adc_pin = (get_raw(Channel::input_voltage) * vref_nominal) / get_raw(Channel::vref_int);
    // Scaled back to ~24V
    return static_cast<uint16_t>(((v_adc_pin * (r1 + r2)) / r2) * 100);
}

uint16_t get_heater_current() {
    // TODO: Dev board schema will be changed
    return 42;
}
} // namespace hal::adc

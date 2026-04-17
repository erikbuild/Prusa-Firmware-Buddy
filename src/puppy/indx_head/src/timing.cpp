#include "timing.hpp"

#include <stm32c0xx_hal.h>

namespace timing {

uint32_t get_timestamp_ms() {
    return HAL_GetTick();
}

uint64_t get_timestamp_us() {
    auto millis = HAL_GetTick();
    // Systick is down counting counter. LOAD contains max counter value which is (system clock / 1000) - 1.
    // So for us it should be 47999. To get number of us we need to divide the LOAD-VAL (what was counted from last millisecond cb)
    // by 48 (that is why we do LOAD + 1 / 1000).

    // Since all the register accesses are "volatile", let's read the constants only once to save memreads
    const auto ticks_per_us = (SysTick->LOAD + 1) / 1000;
    const auto max_val = SysTick->LOAD;
    auto rem = (max_val - SysTick->VAL) / ticks_per_us;
    if (millis != HAL_GetTick()) /*If the clock rolled over retry to get the timestamp again to be sure we have a correct number*/ {
        millis = HAL_GetTick();
        rem = (max_val - SysTick->VAL) / ticks_per_us;
    }
    return millis * 1000 + rem;
}

} // namespace timing

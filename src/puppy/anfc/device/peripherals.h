#pragma once

#ifdef STM32H5
    #include <stm32h5xx.h>
#elifdef STM32C0
    #include <stm32c0xx.h>
#else
    #error
#endif

namespace hal::peripherals {
extern FDCAN_HandleTypeDef hfdcan1;
}

#define TIM3_OVERFLOW 1

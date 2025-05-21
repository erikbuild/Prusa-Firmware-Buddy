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
extern HASH_HandleTypeDef hhash;
} // namespace hal::peripherals

// src/can does not expect the namespace...
using namespace hal::peripherals;

#define TIM3_OVERFLOW 1

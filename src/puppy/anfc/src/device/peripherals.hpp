#pragma once

#include <option/can_bus_type.h>

#include <stm32c0xx_hal.h>

namespace hal::peripherals {
#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_SLX()
extern FDCAN_HandleTypeDef hfdcan1;
#elif CAN_BUS_TYPE_IS_UART()
extern UART_HandleTypeDef huart1;
#else
    #error
#endif
extern SPI_HandleTypeDef hspi1;

#ifdef HASH_ALGOSELECTION_SHA256
// #error dead code found by automatic analyses (see BFW-5461)
extern HASH_HandleTypeDef hhash;
#endif
} // namespace hal::peripherals

// src/can does not expect the namespace...
using namespace hal::peripherals;

#define TIM3_OVERFLOW 1

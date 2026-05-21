#pragma once

#include <stm32c0xx_hal.h>

#define D_LED_USR_GPIO_Port GPIOA
#define D_LED_USR_Pin       GPIO_PIN_7

#define I2C1_SDA_GPIO_Port     GPIOB
#define I2C1_SDA_Pin           GPIO_PIN_7
#define I2C1_SCL_GPIO_Port     GPIOB
#define I2C1_SCL_Pin           GPIO_PIN_8
#define I2C1_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()

#define LDC1612_SD_GPIO_Port         GPIOA
#define LDC1612_SD_Pin               GPIO_PIN_12
#define LDC1612_SD_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define LDC1612_OSC_GPIO_Port         GPIOA
#define LDC1612_OSC_Pin               GPIO_PIN_9
#define LDC1612_OSC_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define LDC1612_INTB_GPIO_Port         GPIOA
#define LDC1612_INTB_Pin               GPIO_PIN_1
#define LDC1612_INTB_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

namespace hal::peripherals {
extern FDCAN_HandleTypeDef hfdcan1;
}

#define TIM3_OVERFLOW 1

#include "hal.hpp"
#include "bsod.h"
#include <device/peripherals.h>
#include <device/hal.h>

extern "C" {
#include <FreeRTOSConfig.h>
}
static_assert(configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY == 5);
#define ISR_PRIORITY_DEFAULT 5 // default ISR priority, used by ISRs that don't need specific ISR priority

#ifdef HASH_ALGOSELECTION_SHA256
    #error
#endif

// SVC_Handler + PendSV_Handler + SysTick_Handler are defined by FreeRTOS

void hal::panic() {
    NVIC_SystemReset();
}

void hal::reset() {
    NVIC_SystemReset();
}

void hal::set_status_led(bool set) {
    HAL_GPIO_WritePin(D_LED_USR_GPIO_Port, D_LED_USR_Pin, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

namespace hal {
void init_clock();
void init_gpio();
void init_tim2();
} // namespace hal

namespace hal::peripherals {
TIM_HandleTypeDef htim2;
} // namespace hal::peripherals

void HAL_MspInit(void) {
    // This is important settings for clock configuration
    // It would loggically fit into init_clock, but STM CubeMX generated project
    // puts it here and we keep it here as well,
    // so it is easier to track possible future changes
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void hal::init() {
    HAL_Init();
    hal::init_clock();
    hal::init_gpio();
    hal::init_tim2();
}

void hal::init_clock() {
    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    {
        /** Initializes the RCC Oscillators according to the specified parameters
         * in the RCC_OscInitTypeDef structure.
         */
        RCC_OscInitTypeDef RCC_OscInitStruct = {};
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        RCC_OscInitStruct.HSEState = RCC_HSE_ON;
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
            bsod_system();
        }
    }

    {
        /** Initializes the CPU, AHB and APB buses clocks
         */
        RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
        RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
        RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
            bsod_system();
        }
    }

    {
        /** Initializes the common peripherals clocks
         */
        RCC_PeriphCLKInitTypeDef PeriphClkInit {};

        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_HSIKER;
        PeriphClkInit.HSIKerClockDivider = RCC_HSIKER_DIV2;

        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
            bsod_system();
        }
    }
}

void hal::init_gpio() {
    // D_LED_USR - PA7
    __HAL_RCC_GPIOA_CLK_ENABLE();
    static constexpr GPIO_InitTypeDef GPIO_InitStruct {
        .Pin = D_LED_USR_Pin,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
        .Alternate = 0,
    };
    HAL_GPIO_Init(D_LED_USR_GPIO_Port, &GPIO_InitStruct);
}

void hal::init_tim2() {
    using namespace hal::peripherals;

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = HAL_RCC_GetHCLKFreq() - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        bsod_system();
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
        bsod_system();
    }
}

extern "C" void hal_panic() {
    hal::panic();
}

extern "C" void NMI_Handler() {
    bsod_system();
}

extern "C" void HardFault_Handler() {
    bsod_system();
}

extern "C" void MemManage_Handler() {
    bsod_system();
}

extern "C" void BusFault_Handler() {
    bsod_system();
}

extern "C" void UsageFault_Handler() {
    bsod_system();
}

extern "C" void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&hal::peripherals::htim2);
}

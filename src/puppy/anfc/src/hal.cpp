#include "hal.hpp"
#include "nfc.hpp"
#include <device/peripherals.h>
#include <device/hal.h>
#include <option/can_bus_type.h>
#include <option/nfc_board_has_left_right_detection_pin.h>

extern "C" {
#include <FreeRTOSConfig.h>
}
static_assert(configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY == 2);
#define ISR_PRIORITY_DEFAULT 2 // default ISR priority, used by ISRs that don't need specific ISR priority

#ifdef HASH_ALGOSELECTION_SHA256
    #error
#endif

// SVC_Handler + PendSV_Handler + SysTick_Handler are defined by FreeRTOS

void hal::panic() {
    asm volatile("bkpt 0");
    NVIC_SystemReset();
}

void hal::reset() {
    NVIC_SystemReset();
}

void hal::set_status_led(bool set) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

hal::BoardOrientation hal::get_board_orientation() {
    if constexpr (option::nfc_board_has_left_right_detection_pin) {
        return (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6) == GPIO_PIN_SET) ? BoardOrientation::right : BoardOrientation::left;
    } else {
        return BoardOrientation::normal;
    }
}

namespace hal {
void init_clock();
#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_SLX()
void init_can();
#elif CAN_BUS_TYPE_IS_UART()
void init_uart();
#else
    #error
#endif
void init_spi();
void init_tim3();
void init_tim2();
void init_adc();
void init_gpio();
} // namespace hal

namespace hal::peripherals {
#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_SLX()
FDCAN_HandleTypeDef hfdcan1;
#elif CAN_BUS_TYPE_IS_UART()
UART_HandleTypeDef huart1;
#else
    #error
#endif
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim2;
ADC_HandleTypeDef hadc1;
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
    hal::init_spi();
#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_SLX()
    hal::init_can();
#elif CAN_BUS_TYPE_IS_UART()
    hal::init_uart();
#else
    #error
#endif
    hal::init_tim2();
    hal::init_tim3();
    hal::init_adc();
}

void hal::init_clock() {
    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    {
        /** Initializes the RCC Oscillators according to the specified parameters
         * in the RCC_OscInitTypeDef structure.
         */
        RCC_OscInitTypeDef RCC_OscInitStruct = {};
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState = RCC_HSI_ON;
        RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
        RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
            hal::panic();
        }
    }

    {
        /** Initializes the CPU, AHB and APB buses clocks
         */
        RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
        RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
            hal::panic();
        }
    }

    {
        /** Initializes the common peripherals clocks
         */
        RCC_PeriphCLKInitTypeDef PeriphClkInit {};

        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_HSIKER;
        PeriphClkInit.HSIKerClockDivider = RCC_HSIKER_DIV2;

        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
            hal::panic();
        }
    }
}

#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_SLX()
void hal::init_can() {
    using namespace hal::peripherals;
    RCC_PeriphCLKInitTypeDef PeriphClkInit {};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN1;
    PeriphClkInit.Fdcan1ClockSelection = RCC_FDCAN1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        hal::panic();
    }

    /* Peripheral clock enable */
    __HAL_RCC_FDCAN1_CLK_ENABLE();

    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init = FDCAN_InitTypeDef {
        .ClockDivider = FDCAN_CLOCK_DIV1,
        .FrameFormat = enable_bit_rate_switch ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_FD_NO_BRS,
        .Mode = FDCAN_MODE_NORMAL,
        .AutoRetransmission = DISABLE,
        .TransmitPause = ENABLE,
        .ProtocolException = ENABLE,
    #if CAN_BUS_TYPE_IS_PUB6()
        /* Target bit rate 125kbit/s sampling point 87.5% */
            .NominalPrescaler = 24,
        .NominalSyncJumpWidth = 64,
        .NominalTimeSeg1 = 13,
        .NominalTimeSeg2 = 2,
        .DataPrescaler = 24,
        .DataSyncJumpWidth = 16,
        .DataTimeSeg1 = 13,
        .DataTimeSeg2 = 2,
    #elif CAN_BUS_TYPE_IS_SLX()
        // Target bit rate 500/1000 kbps
            .NominalPrescaler = 2,
        .NominalSyncJumpWidth = 12,
        .NominalTimeSeg1 = 35,
        .NominalTimeSeg2 = 12,
        .DataPrescaler = 2,
        .DataSyncJumpWidth = 6,
        .DataTimeSeg1 = 17,
        .DataTimeSeg2 = 6,
    #else
        #error
    #endif
        .StdFiltersNbr = 0,
        .ExtFiltersNbr = 0,
        .TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION,
    };

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        hal::panic();
    }

    /**FDCAN1 GPIO Configuration
      PB0      ------> FDCAN1_RX
      PB1      ------> FDCAN1_TX
      */

    __HAL_RCC_GPIOB_CLK_ENABLE();

    static constexpr GPIO_InitTypeDef GPIO_InitStruct {
        .Pin = GPIO_PIN_0 | GPIO_PIN_1,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_HIGH,
        .Alternate = GPIO_AF3_FDCAN1,
    };
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* FDCAN1 interrupt Init */
    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    HAL_NVIC_SetPriority(FDCAN1_IT1_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
}
#elif CAN_BUS_TYPE_IS_UART()
void hal::init_uart() {
    using namespace hal::peripherals;

    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    RCC_PeriphCLKInitTypeDef PeriphClkInit;
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        hal::panic();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();

    static constexpr GPIO_InitTypeDef GPIO_InitStruct = {
        .Pin = GPIO_PIN_9 | GPIO_PIN_10,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
        .Alternate = GPIO_AF1_USART1
    };
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    huart1.Instance = USART1;
    huart1.Init = UART_InitTypeDef {
        .BaudRate = 115200,
        .WordLength = UART_WORDLENGTH_8B,
        .StopBits = UART_STOPBITS_1,
        .Parity = UART_PARITY_NONE,
        .Mode = UART_MODE_TX_RX,
        .HwFlowCtl = UART_HWCONTROL_NONE,
        .OverSampling = UART_OVERSAMPLING_16,
        .OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE,
        .ClockPrescaler = UART_PRESCALER_DIV1
    };
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    if ((HAL_UART_Init(&huart1) != HAL_OK) //
        || (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) //
        || (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) //
        || (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)) {
        hal::panic();
    }
}
#else
    #error
#endif

void hal::init_spi() {
    using namespace hal::peripherals;

    {
        RCC_PeriphCLKInitTypeDef PeriphClkInit {};

        // Initialize the SPI/I2S clock
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2S1;
        PeriphClkInit.I2s1ClockSelection = RCC_I2S1CLKSOURCE_HSIKER;

        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
            hal::panic();
        }
    }

    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    { // NFC SPI SCK, MISO, MOSI
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
            .Mode = GPIO_MODE_AF_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_HIGH,
            .Alternate = GPIO_AF0_SPI1,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    { // NFC chip select pin
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_4,
            .Mode = GPIO_MODE_OUTPUT_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = 0,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    hspi1.Instance = SPI1;
    hspi1.Init = {
        .Mode = SPI_MODE_MASTER,
        .Direction = SPI_DIRECTION_2LINES,
        .DataSize = SPI_DATASIZE_8BIT,
        .CLKPolarity = SPI_POLARITY_LOW,
        .CLKPhase = SPI_PHASE_2EDGE,
        .NSS = SPI_NSS_SOFT,
        .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4,
        .FirstBit = SPI_FIRSTBIT_MSB,
        .TIMode = SPI_TIMODE_DISABLE,
        .CRCCalculation = SPI_CRCCALCULATION_DISABLE,
        .CRCPolynomial = 0x7,
        .CRCLength = SPI_CRC_LENGTH_DATASIZE,
        .NSSPMode = SPI_NSS_PULSE_DISABLE,
    };

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        hal::panic();
    }

    HAL_NVIC_SetPriority(SPI1_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

void hal::init_tim3() {
    using namespace hal::peripherals;

    GPIO_InitTypeDef GPIO_InitStruct {};
    TIM_ClockConfigTypeDef sClockSourceConfig = {};
    TIM_MasterConfigTypeDef sMasterConfig = {};
    TIM_OC_InitTypeDef sConfigOC = {};

    // FS_LED_PWM_R - PA8 - TIM3_CH3
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF3_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // FS_LED_PWM2_L - PC6 - TIM3_CH1
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF3_TIM3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // common part for STM32C0 timer initialization
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 65535;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        hal::panic();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
        hal::panic();
    }
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        hal::panic();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
        hal::panic();
    }
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        hal::panic();
    }
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
        hal::panic();
    }
}

void hal::init_tim2() {
    using namespace hal::peripherals;

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 48'000'000;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        hal::panic();
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
        hal::panic();
    }
}

void hal::init_adc() {
    using namespace hal::peripherals;

    ADC_ChannelConfTypeDef sConfig = {};

    // no need to configure GPIOs, default configuration is working properly for ADC
    __HAL_RCC_ADC_CLK_ENABLE();
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.LowPowerAutoPowerOff = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_1CYCLE_5;
    hadc1.Init.OversamplingMode = DISABLE;
    hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        hal::panic();
    }

    // Ambient Temperature - ADC1_IN0 - PA0
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        hal::panic();
    }

    // FS_ADC_R - ADC1_IN1 - PA1
    sConfig.Channel = ADC_CHANNEL_1;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        hal::panic();
    }

    // FS_ADC_L - ADC1_IN2 - PA2
    sConfig.Channel = ADC_CHANNEL_2;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        hal::panic();
    }
}

void hal::init_gpio() {
    // NFC_INT PA3
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_3,
            .Mode = GPIO_MODE_IT_RISING,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_HIGH,
            .Alternate = 0,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(EXTI2_3_IRQn, ISR_PRIORITY_DEFAULT, 0);
        HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
    }

    // STATUS_LED PA12
    {
        // __HAL_RCC_GPIOA_CLK_ENABLE(); not needed due to clock enabling for PA3 pin
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_12,
            .Mode = GPIO_MODE_OUTPUT_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_HIGH,
            .Alternate = 0,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    // LEFT_OR_RIGHT_Pin PB6
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_6,
            .Mode = GPIO_MODE_INPUT,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_HIGH,
            .Alternate = 0,
        };
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

extern "C" void hal_panic() {
    hal::panic();
}

extern "C" void NMI_Handler() {
    hal::panic();
}

extern "C" void HardFault_Handler() {
    hal::panic();
}

extern "C" void MemManage_Handler() {
    hal::panic();
}

extern "C" void BusFault_Handler() {
    hal::panic();
}

extern "C" void UsageFault_Handler() {
    hal::panic();
}

extern "C" void EXTI2_3_IRQHandler() {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}

extern "C" void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_3) {
        nfc::irq();
    }
}

extern "C" void SPI1_IRQHandler(void) {
    HAL_SPI_IRQHandler(&hspi1);
}

#if CAN_BUS_TYPE_IS_PUB6() || CAN_BUS_TYPE_IS_SLX()
/**
 * @brief This function handles FDCAN1 interrupt 0.
 */

extern "C" void FDCAN1_IT0_IRQHandler(void) {
    HAL_FDCAN_IRQHandler(&hal::peripherals::hfdcan1);
}

/**
 * @brief This function handles FDCAN1 interrupt 1.
 */

extern "C" void FDCAN1_IT1_IRQHandler(void) {
    HAL_FDCAN_IRQHandler(&hal::peripherals::hfdcan1);
}
#elif CAN_BUS_TYPE_IS_UART()
/**
 * @brief This function handles UART interrupt.
 */
extern "C" void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&hal::peripherals::huart1);
}
#else
    #error
#endif

extern "C" void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&hal::peripherals::htim2);
}

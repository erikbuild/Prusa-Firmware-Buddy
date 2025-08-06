#include "hal.hpp"
#include "nfc.hpp"
#include <device/peripherals.h>
#include <device/hal.h>
#include <option/can_bus_type.h>

extern "C" {
#include <FreeRTOSConfig.h>
}
static_assert(configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY == 2);
#define ISR_PRIORITY_DEFAULT 2 // default ISR priority, used by ISRs that don't need specific ISR priority

// SVC_Handler + PendSV_Handler + SysTick_Handler are defined by FreeRTOS on H5.
// On C0 we need to rename them to cmsis alternatives for it to work.
// It's because our version of FreeRTOS 10.6.2 is mid migration of port configuration.
// Newer version define them like for H5 (Cortex M33).

#ifndef HASH_AlgoSelection_SHA256
    #error
#endif

void hal::panic() {
    asm volatile("bkpt 0");
    NVIC_SystemReset();
}

void hal::reset() {
    NVIC_SystemReset();
}

void hal::set_status_led(bool set) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, set ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

namespace hal {
void init_clock();
void init_can();
void init_spi();
void init_tim3();
void init_adc();
void init_gpio();
void init_tim2();

#ifdef HASH_ALGOSELECTION_SHA256
void init_hash();
#endif
} // namespace hal

namespace hal::peripherals {
FDCAN_HandleTypeDef hfdcan1;
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim2;
ADC_HandleTypeDef hadc1;
HASH_HandleTypeDef hhash;
} // namespace hal::peripherals

void HAL_MspInit(void) {
}

void hal::init() {
    HAL_Init();
    hal::init_clock();
    hal::init_gpio();
    hal::init_spi();
    hal::init_can();
    hal::init_tim2();
    hal::init_tim3();
    hal::init_adc();
    hal::init_hash();
}

void hal::init_clock() {
    RCC_OscInitTypeDef RCC_OscInitStruct {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct {};

    /** Configure the main internal regulator output voltage
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     * Target HCLK frequency is 240MHz
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_CSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.CSIState = RCC_CSI_ON;
    RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 30;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        hal::panic();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
        | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        hal::panic();
    }

    /** Configure the programming delay
     */
    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

void hal::init_can() {
    using namespace hal::peripherals;

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct {};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL1Q;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        hal::panic();
    }

    /* Peripheral clock enable */
    __HAL_RCC_FDCAN_CLK_ENABLE();

    /* Target bit rate 125kbit/s sampling point 87.5% */
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init = FDCAN_InitTypeDef {
        .ClockDivider = FDCAN_CLOCK_DIV4,
        .FrameFormat = enable_bit_rate_switch ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_FD_NO_BRS,
        .Mode = FDCAN_MODE_NORMAL,
        .AutoRetransmission = DISABLE,
        .TransmitPause = ENABLE,
        .ProtocolException = ENABLE,
        .NominalPrescaler = 30,
        .NominalSyncJumpWidth = 64,
        .NominalTimeSeg1 = 13,
        .NominalTimeSeg2 = 2,
        .DataPrescaler = 30,
        .DataSyncJumpWidth = 16,
        .DataTimeSeg1 = 13,
        .DataTimeSeg2 = 2,
        .StdFiltersNbr = 0,
        .ExtFiltersNbr = 0,
        .TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION,
    };
    static_assert(CAN_BUS_TYPE_IS_PUB6());

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        hal::panic();
    }

    /**FDCAN1 GPIO Configuration
      PB8      ------> FDCAN1_RX
      PB7      ------> FDCAN1_TX
      */

    __HAL_RCC_GPIOB_CLK_ENABLE();

    static constexpr GPIO_InitTypeDef GPIO_InitStruct {
        .Pin = GPIO_PIN_7 | GPIO_PIN_8,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_HIGH,
        .Alternate = GPIO_AF9_FDCAN1,
    };
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* FDCAN1 interrupt Init */
    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    HAL_NVIC_SetPriority(FDCAN1_IT1_IRQn, ISR_PRIORITY_DEFAULT, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
}

void hal::init_spi() {
    using namespace hal::peripherals;

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct {};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI1;
    PeriphClkInitStruct.PLL2.PLL2Source = RCC_PLL2_SOURCE_CSI;
    PeriphClkInitStruct.PLL2.PLL2M = 1;
    PeriphClkInitStruct.PLL2.PLL2N = 35;
    PeriphClkInitStruct.PLL2.PLL2P = 7;
    PeriphClkInitStruct.PLL2.PLL2Q = 4;
    PeriphClkInitStruct.PLL2.PLL2R = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2_VCIRANGE_2;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2_VCORANGE_WIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
    PeriphClkInitStruct.PLL2.PLL2ClockOut = RCC_PLL2_DIVP;
    PeriphClkInitStruct.Spi1ClockSelection = RCC_SPI1CLKSOURCE_PLL2P;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        hal::panic();
    }

    __HAL_RCC_SPI1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /** SPI1 GPIO Configuration
    PA1     ------> SPI1_NSS
    PA2     ------> SPI1_SCK
    PA3     ------> SPI1_MISO
    PA4     ------> SPI1_MOSI
    */

    { // NFC SPI SCK. MISO, MOSI
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4,
            .Mode = GPIO_MODE_AF_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = GPIO_AF4_SPI1,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    { // NFC chip select pin
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_1,
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
        .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2,
        .FirstBit = SPI_FIRSTBIT_MSB,
        .TIMode = SPI_TIMODE_DISABLE,
        .CRCCalculation = SPI_CRCCALCULATION_DISABLE,
        .CRCPolynomial = 0x7,
        .CRCLength = SPI_CRC_LENGTH_10BIT,
        .NSSPMode = SPI_NSS_PULSE_DISABLE,
        .NSSPolarity = SPI_NSS_POLARITY_LOW,
        .FifoThreshold = SPI_FIFO_THRESHOLD_01DATA,
        .TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN,
        .RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN,
        .MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE,
        .MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE,
        .MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE,
        .MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE,
        .IOSwap = SPI_IO_SWAP_DISABLE,
        .ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY,
        .ReadyPolarity = SPI_RDY_POLARITY_HIGH,
    };
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        hal::panic();
    }
}

void hal::init_tim2() {
    using namespace hal::peripherals;

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 240'000'000;
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

void hal::init_tim3() {
    using namespace hal::peripherals;

    GPIO_InitTypeDef GPIO_InitStruct {};
    TIM_ClockConfigTypeDef sClockSourceConfig = {};
    TIM_MasterConfigTypeDef sMasterConfig = {};
    TIM_OC_InitTypeDef sConfigOC = {};

    // LEDR_PWM - PB6
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
        hal::panic();
    }
}

void hal::init_adc() {
    using namespace hal::peripherals;

    ADC_ChannelConfTypeDef sConfig = {};

    // no need to configure GPIOs, default configuration is working properly for ADC
    __HAL_RCC_ADCDAC_CONFIG(RCC_ADCDACCLKSOURCE_HSI);
    __HAL_RCC_ADC_CLK_ENABLE();
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.SamplingMode = ADC_SAMPLING_MODE_NORMAL;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        hal::panic();
    }

    // ADC1_IN0 - PA0
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        hal::panic();
    }
}

void hal::init_gpio() {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    { // NFC_INT
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_5,
            .Mode = GPIO_MODE_IT_RISING,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = 0,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        HAL_NVIC_SetPriority(EXTI5_IRQn, ISR_PRIORITY_DEFAULT, 0);
        HAL_NVIC_EnableIRQ(EXTI5_IRQn);
    }

    { // Status LED
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_6,
            .Mode = GPIO_MODE_OUTPUT_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = 0,
        };

        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void hal::init_hash() {
    using namespace hal::peripherals;

    hhash.Instance = HASH;
    hhash.Init.DataType = HASH_BYTE_SWAP;
    hhash.Init.Algorithm = HASH_ALGOSELECTION_SHA256;
    if (HAL_HASH_Init(&hhash) != HAL_OK) {
        panic();
    }
}

extern "C" void HAL_HASH_MspInit(HASH_HandleTypeDef *hhash) {
    (void)hhash;
    __HAL_RCC_HASH_CLK_ENABLE();
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

extern "C" void EXTI5_IRQHandler() {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
}

extern "C" void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_5) {
        nfc::irq();
    }
}
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

extern "C" void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&hal::peripherals::htim2);
}

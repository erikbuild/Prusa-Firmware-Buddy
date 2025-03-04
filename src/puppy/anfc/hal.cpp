#include "hal.hpp"
#include "nfc.hpp"

#ifdef STM32H5
    #include <stm32h5xx.h>
#elifdef STM32C0
    #include <stm32c0xx.h>
#else
    #error
#endif

void hal::panic() {
    hal::status_led_on();
    asm volatile("bkpt 0");
    NVIC_SystemReset();
}

void hal::status_led_on() {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
}

void hal::status_led_off() {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
}

void hal::delay(uint32_t ms) {
    HAL_Delay(ms);
}

namespace hal {
void init_clock();
void init_can();
void init_spi();
void init_gpio();
} // namespace hal

namespace hal::peripherals {
FDCAN_HandleTypeDef hfdcan1;
SPI_HandleTypeDef hspi1;
} // namespace hal::peripherals

void hal::init() {
    HAL_Init();
    hal::init_clock();
    hal::init_gpio();
    hal::init_spi();
    hal::init_can();
}

void hal::init_clock() {
#ifdef STM32H5
    RCC_OscInitTypeDef RCC_OscInitStruct {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct {};

    /** Configure the main internal regulator output voltage
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSI
        | RCC_OSCILLATORTYPE_CSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.CSIState = RCC_CSI_ON;
    RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 32;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
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
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
        hal::panic();
    }

    /** Configure the programming delay
     */
    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
#else
    #error "No hal::init_clock() implementation"
#endif
}

void hal::init_can() {
    using namespace hal::peripherals;
#ifdef STM32H5
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct {};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL1Q;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        hal::panic();
    }

    /* Peripheral clock enable */
    __HAL_RCC_FDCAN_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**FDCAN1 GPIO Configuration
    PB15     ------> FDCAN1_TX
    PA8     ------> FDCAN1_RX
    */
    {
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_15,
            .Mode = GPIO_MODE_AF_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = GPIO_AF9_FDCAN1,
        };
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }

    {
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_8,
            .Mode = GPIO_MODE_AF_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = GPIO_AF9_FDCAN1,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = DISABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 16;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 1;
    hfdcan1.Init.NominalTimeSeg2 = 1;
    hfdcan1.Init.DataPrescaler = 1;
    hfdcan1.Init.DataSyncJumpWidth = 1;
    hfdcan1.Init.DataTimeSeg1 = 1;
    hfdcan1.Init.DataTimeSeg2 = 1;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        hal::panic();
    }

#else
    #error "No hal::init_can() implementation"
#endif
}

void hal::init_spi() {
    using namespace hal::peripherals;
#ifdef STM32H5
    GPIO_InitTypeDef GPIO_InitStruct {};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct {};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI1;
    PeriphClkInitStruct.Spi1ClockSelection = RCC_SPI1CLKSOURCE_PLL1Q;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        hal::panic();
    }

    __HAL_RCC_SPI1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 0x7;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
    hspi1.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
    hspi1.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        hal::panic();
    }
#else
    #error "No hal::init_spi() implementation"
#endif
}

void hal::init_gpio() {
#ifdef STM32H5
    GPIO_InitTypeDef GPIO_InitStruct {};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI5_IRQn);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#else
    #error "No hal::init_gpio() implementation"
#endif
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

extern "C" void SysTick_Handler() {
    HAL_IncTick();
}

extern "C" void EXTI5_IRQHandler() {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
}

extern "C" void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_5) {
        nfc::irq();
    }
}

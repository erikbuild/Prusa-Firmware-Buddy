#include "hal.hpp"
#include "nfc.hpp"
#include "device/peripherals.h"

#ifdef STM32H5
    #include <stm32h5xx.h>
#elifdef STM32C0
    #include <stm32c0xx.h>
#else
    #error
#endif

extern "C" {
#include <FreeRTOSConfig.h>
}
static_assert(configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY == 2);
#define ISR_PRIORITY_DEFAULT 2 // default ISR priority, used by ISRs that don't need specific ISR priority

// SVC_Handler + PendSV_Handler + SysTick_Handler are defined by FreeRTOS

const std::span<std::byte> hal::memory::peripheral_address_region(reinterpret_cast<std::byte *>(PERIPH_BASE_NS), 0x10000000);

void hal::panic() {
    asm volatile("bkpt 0");
    NVIC_SystemReset();
}

void hal::reset() {
    NVIC_SystemReset();
}

void hal::set_status_led(bool set) {
    // Note: the LED logic is inverted on the board
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, set ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

namespace hal {
void init_clock();
void init_can();
void init_spi();
void init_gpio();
void init_hash();
} // namespace hal

namespace hal::peripherals {
FDCAN_HandleTypeDef hfdcan1;
HASH_HandleTypeDef hhash;
SPI_HandleTypeDef hspi1;
} // namespace hal::peripherals

void hal::init() {
    HAL_Init();
    hal::init_clock();
    hal::init_gpio();
    hal::init_spi();
    hal::init_can();
    hal::init_hash();
}

void hal::init_clock() {
#ifdef STM32H5
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

    /* Target bit rate 125kbit/s sampling point 87.5% */
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init = FDCAN_InitTypeDef {
        .ClockDivider = FDCAN_CLOCK_DIV1,
        .FrameFormat = enable_bit_rate_switch ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_FD_NO_BRS,
        .Mode = FDCAN_MODE_NORMAL,
        .AutoRetransmission = DISABLE,
        .TransmitPause = ENABLE,
        .ProtocolException = ENABLE,
        .NominalPrescaler = 120,
        .NominalSyncJumpWidth = 64,
        .NominalTimeSeg1 = 13,
        .NominalTimeSeg2 = 2,
        .DataPrescaler = 15,
        .DataSyncJumpWidth = 6,
        .DataTimeSeg1 = 13,
        .DataTimeSeg2 = 2,
        .StdFiltersNbr = 0,
        .ExtFiltersNbr = 0,
        .TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION,
    };

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

#else
    #error "No hal::init_can() implementation"
#endif
}

void hal::init_spi() {
    using namespace hal::peripherals;
#ifdef STM32H5
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

    {
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4,
            .Mode = GPIO_MODE_AF_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = GPIO_AF4_SPI1,
        };
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
    {
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
#else
    #error "No hal::init_spi() implementation"
#endif
}

void hal::init_gpio() {
#ifdef STM32H5
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    { // NFC Interupt
        static constexpr GPIO_InitTypeDef GPIO_InitStruct {
            .Pin = GPIO_PIN_5,
            .Mode = GPIO_MODE_IT_RISING,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
            .Alternate = 0
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
            .Alternate = 0
        };

        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }

#else
    #error "No hal::init_gpio() implementation"
#endif
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

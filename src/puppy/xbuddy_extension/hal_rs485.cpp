/// @file
#include "hal_rs485.hpp"

#include <atomic>
#include <freertos/binary_semaphore.hpp>
#include <stm32h5xx_hal.h>

static UART_HandleTypeDef huart;
alignas(uint16_t) static std::byte rx_buf[256];
static volatile size_t rx_len;
static freertos::BinarySemaphore tx_semaphore;
static std::atomic<bool> ore_occurred;

extern "C" void USART3_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart);
}

static void uart_init() {
    huart.Instance = USART3;
    huart.Init.BaudRate = 230'400;
    huart.Init.WordLength = UART_WORDLENGTH_8B;
    huart.Init.StopBits = UART_STOPBITS_1;
    huart.Init.Parity = UART_PARITY_NONE;
    huart.Init.Mode = UART_MODE_TX_RX;
    huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart.Init.OverSampling = UART_OVERSAMPLING_16;
    huart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart) != HAL_OK) {
        abort();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        abort();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        abort();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart) != HAL_OK) {
        abort();
    }
    if (HAL_RS485Ex_Init(&huart, UART_DE_POLARITY_HIGH, 0x1f, 0x1f) != HAL_OK) {
        abort();
    }
}

void hal::rs485::init() {
    uart_init();
}

void hal::rs485::msp_init(void *handle) {
    auto huart = static_cast<UART_HandleTypeDef *>(handle);
    if (huart->Instance == USART3) {
        RCC_PeriphCLKInitTypeDef PeriphClkInitStruct {};
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3;
        PeriphClkInitStruct.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            abort();
        }
        __HAL_RCC_USART3_CLK_ENABLE();

        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct {};
        GPIO_InitStruct.Pin = GPIO_PIN_8;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF13_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_14;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
}

void hal::rs485::rx_callback(void *handle, uint16_t size) {
    if (static_cast<UART_HandleTypeDef *>(handle) == &huart) {
        if (size == 0) {
            HAL_UARTEx_ReceiveToIdle_IT(&huart, (uint8_t *)rx_buf, sizeof(rx_buf));
        } else {
            rx_len = size;
            tx_semaphore.release_from_isr();
        }
    }
}

void hal::rs485::tx_callback(void *handle) {
    if (static_cast<UART_HandleTypeDef *>(handle) == &huart) {
        HAL_UARTEx_ReceiveToIdle_IT(&huart, (uint8_t *)rx_buf, sizeof(rx_buf));
    }
}

void hal::rs485::error_callback(void *handle) {
    auto huart = static_cast<UART_HandleTypeDef *>(handle);
    // TODO add ORE handling for MMU too
    if (huart->ErrorCode & HAL_UART_ERROR_ORE) {
        if (huart == &::huart) {
            ore_occurred = true;
        }
    }
}

void hal::rs485::start_receiving() {
    HAL_UART_TxCpltCallback(&huart);
}

std::span<std::byte> hal::rs485::receive() {
    tx_semaphore.acquire();
    return { rx_buf, rx_len };
}

std::span<std::byte> hal::rs485::receive_timeout(uint32_t timeout_ms) {
    if (tx_semaphore.try_acquire_for(timeout_ms)) {
        return { rx_buf, rx_len };
    }
    return {};
}

void hal::rs485::transmit_and_then_start_receiving(std::span<std::byte> payload) {
    HAL_UART_Transmit_IT(&huart, (uint8_t *)payload.data(), payload.size());
}

void hal::rs485::housekeeping() {
    if (ore_occurred) {
        HAL_UART_AbortReceive_IT(&huart);
        HAL_UARTEx_ReceiveToIdle_IT(&huart, (uint8_t *)rx_buf, sizeof(rx_buf));
        ore_occurred = false;
    }
}

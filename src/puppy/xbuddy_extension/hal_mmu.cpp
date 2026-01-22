/// @file
#include "hal_mmu.hpp"

#include "hal_gpio_expander.hpp"
#include <freertos/binary_semaphore.hpp>
#include <freertos/stream_buffer.hpp>
#include <stm32h5xx_hal.h>

static UART_HandleTypeDef huart;
static std::byte rx_byte;
static std::span<const std::byte> rx_byte_span { &rx_byte, 1 };
static freertos::StreamBuffer<32> rx_buffer;
static freertos::BinarySemaphore tx_semaphore;

extern "C" void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart);
}

static void uart_init() {
    huart.Instance = USART2;
    huart.Init.BaudRate = 115'200;
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
    HAL_UARTEx_ReceiveToIdle_IT(&huart, (uint8_t *)&rx_byte, 1);
}

static void nreset_pin_init() {
    GPIO_InitTypeDef GPIO_InitStruct;
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void hal::mmu::init() {
    uart_init();
    nreset_pin_init();
    hal::mmu::nreset_pin_set(false);
}

void hal::mmu::msp_init(void *handle) {
    auto huart = static_cast<UART_HandleTypeDef *>(handle);
    if (huart->Instance == USART2) {
        RCC_PeriphCLKInitTypeDef PeriphClkInitStruct {};
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2;
        PeriphClkInitStruct.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            abort();
        }
        __HAL_RCC_USART2_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct {};
        GPIO_InitStruct.Pin = GPIO_PIN_15;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF9_USART2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_4;
        GPIO_InitStruct.Alternate = GPIO_AF13_USART2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }
}

void hal::mmu::rx_callback(void *handle, uint16_t size) {
    if (static_cast<UART_HandleTypeDef *>(handle) == &huart) {
        if (size) {
            // If the buffer is full, we just start dropping bytes and that's ok.
            (void)rx_buffer.send_from_isr(rx_byte_span);
        }
        HAL_UARTEx_ReceiveToIdle_IT(&huart, (uint8_t *)&rx_byte, 1);
    }
}

void hal::mmu::tx_callback(void *handle) {
    if (static_cast<UART_HandleTypeDef *>(handle) == &huart) {
        tx_semaphore.release_from_isr();
    }
}

void hal::mmu::transmit(std::span<const std::byte> payload) {
    HAL_UART_Transmit_IT(&huart, (const uint8_t *)payload.data(), payload.size());
    tx_semaphore.acquire();
}

std::span<std::byte> hal::mmu::receive(std::span<std::byte> buffer) {
    return rx_buffer.receive(buffer);
}

void hal::mmu::flush() {
    std::byte buf[8];
    while (!receive(buf).empty()) {
    }
}

void hal::mmu::power_pin_set(bool b) {
    hal::gpio_expander::write(hal::gpio_expander::Pin::mmu_power, b);
}

void hal::mmu::nreset_pin_set(bool b) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool hal::mmu::power_pin_get() {
    return hal::gpio_expander::read(hal::gpio_expander::Pin::mmu_power);
}

bool hal::mmu::nreset_pin_get() {
    return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET;
}

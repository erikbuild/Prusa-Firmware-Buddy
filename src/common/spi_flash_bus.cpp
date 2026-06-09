#include <common/spi_flash_bus.hpp>

#include <cstring>
#include <buddy/ccm_thread.hpp>
#include <device/peripherals.hpp>
#include <logging/log.hpp>
#include "timing_precise.hpp"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"

static constexpr uint32_t timeout_ms = 1000;

SpiFlashBus &SpiFlashBus::instance() {
    static SpiFlashBus instance(spi_handle_flash);
    return instance;
}

static bool memory_supports_dma_transfer(const void *location) {
    return (uintptr_t)location >= 0x20000000;
}

static bool dma_is_available() {
    return !xPortIsInsideInterrupt() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
}

SpiFlashBus::SpiFlashBus(SPI_HandleTypeDef *spi_handle)
    : spi_handle(spi_handle) {}

void SpiFlashBus::select(const buddy::hw::OutputPin &cs) {
    cs.write(buddy::hw::Pin::State::low);
    delay_ns_precise<cs_select_delay_ns>();
}

void SpiFlashBus::deselect(const buddy::hw::OutputPin &cs) {
    cs.write(buddy::hw::Pin::State::high);
    delay_ns_precise<cs_deselect_delay_ns>();
}

void SpiFlashBus::release_dma_from_isr(HAL_StatusTypeDef status) {
    dma_status = status;
    dma_semaphore.release_from_isr();
}

HAL_StatusTypeDef SpiFlashBus::receive_dma(uint8_t *buffer, uint32_t len) {
    assert(can_be_used_by_dma(buffer));
    const HAL_StatusTypeDef status = HAL_SPI_Receive_DMA(spi_handle, buffer, len);
    if (status == HAL_OK) {
        dma_semaphore.acquire();
        return dma_status;
    } else {
        return status;
    }
}

HAL_StatusTypeDef SpiFlashBus::send_dma(const uint8_t *buffer, uint32_t len) {
    assert(can_be_used_by_dma(buffer));
    const HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(spi_handle, (uint8_t *)buffer, len);
    if (status == HAL_OK) {
        dma_semaphore.acquire();
        return dma_status;
    } else {
        return status;
    }
}

LOG_COMPONENT_DEF(FlashBus, logging::Severity::info);

#if 0
struct Measure {
    const char *fmt;
    uint32_t len;
    uint32_t cyccnt;
    Measure(const char *fmt_, uint32_t len_)
        : fmt { fmt_ }
        , len { len_ }
        , cyccnt { DWT->CYCCNT } {}
    ~Measure() {
        log_info(FlashBus, fmt, len, DWT->CYCCNT - cyccnt);
    }
};
#else
struct Measure {
    Measure(...) {}
};
#endif

void SpiFlashBus::receive(uint8_t *buffer, uint32_t len) {
    if (current_error != 0) {
        return;
    }

    if (len > 4 && dma_is_available()) {
        if (memory_supports_dma_transfer(buffer)) {
            Measure _ { "recv dma-fast %u %u", len };
            current_error = receive_dma(buffer, len);
            return;
        } else {
            Measure _ { "recv dma-slow %u %u", len };
            while (current_error == 0 && len) {
                uint32_t block_len = len > sizeof(block_buffer) ? sizeof(block_buffer) : len;
                current_error = receive_dma(block_buffer, block_len);
                memcpy(buffer, block_buffer, block_len);
                buffer += block_len;
                len -= block_len;
            }
        }
    } else {
        Measure _ { "recv dma-none %u %u", len };
        current_error = HAL_SPI_Receive(spi_handle, buffer, len, timeout_ms);
    }
}

uint8_t SpiFlashBus::receive_byte() {
    uint8_t byte;
    receive(&byte, 1);
    return byte;
}

void SpiFlashBus::send(const uint8_t *buffer, uint32_t len) {
    if (current_error != 0) {
        return;
    }

    if (len > 4 && dma_is_available()) {
        if (memory_supports_dma_transfer(buffer)) {
            Measure _ { "send dma-fast %u %u", len };
            current_error = send_dma(buffer, len);
        } else {
            Measure _ { "send dma-slow %u %u", len };
            while (current_error == 0 && len) {
                uint32_t block_len = len > sizeof(block_buffer) ? sizeof(block_buffer) : len;
                memcpy(block_buffer, buffer, block_len);
                current_error = send_dma(block_buffer, block_len);
                buffer += block_len;
                len -= block_len;
            }
        }
    } else {
        Measure _ { "send dma-none %u %u", len };
        current_error = HAL_SPI_Transmit(spi_handle, (uint8_t *)buffer, len, timeout_ms);
    }
}

void SpiFlashBus::send_byte(uint8_t byte) {
    send(&byte, sizeof(byte));
}

void SpiFlashBus::set_error(int error) {
    current_error = error;
}

int SpiFlashBus::fetch_error() {
    int error = current_error;
    current_error = 0;
    return error;
}

void SpiFlashBus::reinit_for_crash_dump() {
    (void)HAL_SPI_Abort(spi_handle);
    current_error = 0;
}

void SpiFlashBus::on_tx_complete() {
    release_dma_from_isr(HAL_OK);
}

void SpiFlashBus::on_rx_complete() {
    release_dma_from_isr(HAL_OK);
}

void SpiFlashBus::on_error() {
    release_dma_from_isr(HAL_ERROR);
}

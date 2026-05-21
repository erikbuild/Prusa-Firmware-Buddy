#include "i2c_manager.hpp"
#include <bsod.h>
#include <device/peripherals.hpp>
#include <mutex>

namespace i2c {

template <typename F>
Result Manager::with_retries(F &&operation) {
    std::lock_guard<freertos::Mutex> lock(mutex);
    auto result = Result::busy_after_retries;
    for (int retries = max_retries; retries > 0; --retries) {
        result = operation();
        if (result == Result::ok) {
            break;
        }
    }
    return result;
}

static Result process_result(HAL_StatusTypeDef result) {
    switch (result) {
    case HAL_OK:
        return Result::ok;
    case HAL_ERROR:
        return Result::error;
    case HAL_BUSY:
        return Result::busy_after_retries;
    case HAL_TIMEOUT:
        return Result::timeout;
    }

    bsod_system();
    return Result::error; // will not get here, just prevent warning
}

Result Manager::single_transmit(uint16_t dev_address, uint8_t *data_p, uint16_t size) {
    auto hal_result = HAL_I2C_Master_Transmit_IT(&hi2c, translate_address(dev_address), data_p, size);
    if (hal_result != HAL_OK) {
        return process_result(hal_result);
    }
    semaphore.acquire();
    return isr_result;
}

Result Manager::transmit(uint16_t dev_address, uint8_t *data_p, uint16_t size) {
    return with_retries([&] { return single_transmit(dev_address, data_p, size); });
}

Result Manager::single_receive(uint16_t dev_address, uint8_t *data_p, uint16_t size) {
    auto hal_result = HAL_I2C_Master_Receive_IT(&hi2c, translate_address(dev_address), data_p, size);
    if (hal_result != HAL_OK) {
        return process_result(hal_result);
    }
    semaphore.acquire();
    return isr_result;
}

Result Manager::receive(uint16_t dev_address, uint8_t *data_p, uint16_t size) {
    return with_retries([&] { return single_receive(dev_address, data_p, size); });
}

Result Manager::single_mem_write(uint16_t dev_address, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data_p, uint16_t size) {
    auto hal_result = HAL_I2C_Mem_Write_IT(&hi2c, translate_address(dev_address), mem_address, mem_add_size, data_p, size);
    if (hal_result != HAL_OK) {
        return process_result(hal_result);
    }
    semaphore.acquire();
    return isr_result;
}

Result Manager::mem_write(uint16_t dev_address, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data_p, uint16_t size) {
    return with_retries([&] { return single_mem_write(dev_address, mem_address, mem_add_size, data_p, size); });
}

Result Manager::single_mem_read(uint16_t dev_address, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data_p, uint16_t size) {
    auto hal_result = HAL_I2C_Mem_Read_IT(&hi2c, translate_address(dev_address), mem_address, mem_add_size, data_p, size);
    if (hal_result != HAL_OK) {
        return process_result(hal_result);
    }
    semaphore.acquire();
    return isr_result;
}

Result Manager::mem_read(uint16_t dev_address, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data_p, uint16_t size) {
    return with_retries([&] { return single_mem_read(dev_address, mem_address, mem_add_size, data_p, size); });
}

} // namespace i2c

#include "nfc.hpp"
#include "hal.hpp"
#include "device/peripherals.h"

#include <st25r39xxb/ST25R39XXB.hpp>
#include <freertos/timing.hpp>
#include <freertos/binary_semaphore.hpp>

#if defined(STM32H5)
    #include <stm32h5xx_hal.h>
#elif defined(STM32C0)
    #include <stm32c0xx_hal.h>
#else
    #error
#endif

namespace nfc {
namespace {
    class HWImpl : public st25r39xxb::SpiInterface {
    public:
        HWImpl(SPI_HandleTypeDef *spi, GPIO_TypeDef *gpio_port, uint16_t gpio_pin)
            : spi(spi)
            , gpio_port(gpio_port)
            , gpio_pin(gpio_pin) {}

        void chip_select() final {
            HAL_GPIO_WritePin(gpio_port, gpio_pin, GPIO_PIN_RESET);
        }

        void chip_deselect() final {
            HAL_GPIO_WritePin(gpio_port, gpio_pin, GPIO_PIN_SET);
        }

        void unsafe_transmit(const std::span<const std::byte> &tx) final {
            [[maybe_unused]] const auto res = HAL_SPI_Transmit(spi, reinterpret_cast<const uint8_t *>(tx.data()), tx.size(), 100);
            if (res != HAL_OK) {
                hal::panic();
            }
        }

        void unsafe_receive(const std::span<std::byte> &rx) final {
            [[maybe_unused]] const auto res = HAL_SPI_Receive(spi, reinterpret_cast<uint8_t *>(rx.data()), rx.size(), 100);
            if (res != HAL_OK) {
                hal::panic();
            }
        }

    protected:
        SPI_HandleTypeDef *spi;
        GPIO_TypeDef *gpio_port;
        uint16_t gpio_pin;
    };

    struct SysImpl : public st25r39xxb::SystemInterface {
        void delay(uint32_t delay_ms) final {
            freertos::delay(delay_ms);
        }

        uint32_t await_interrupt(uint32_t timeout_ms) final {
            const auto start = freertos::millis();
            irq_triggered.try_acquire_for(timeout_ms);
            const auto passed = freertos::millis() - start;
            if (passed > timeout_ms) {
                return 0;
            }
            return timeout_ms - passed;
        }

        void trigger_interrupt() final {
            irq_triggered.release_from_isr();
        }

    protected:
        freertos::BinarySemaphore irq_triggered;
    };

    namespace nfcr1 {
        HWImpl hw_impl(&hal::peripherals::hspi1, GPIOA, GPIO_PIN_1);
        SysImpl sys_impl {};
    } // namespace nfcr1
} // namespace
st25r39xxb::ST25R39XXB reader_1 { nfcr1::hw_impl, nfcr1::sys_impl };
} // namespace nfc

void nfc::readers_init() {
    auto init_res = nfc::reader_1.init();
    if (!init_res.has_value()) {
        hal::panic();
    }
}

void nfc::irq() {
    nfcr1::sys_impl.trigger_interrupt();
}

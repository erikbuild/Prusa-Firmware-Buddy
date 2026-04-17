#pragma once

#include <stm32c0xx_hal.h>
#include <fpm/fixed.hpp>

#include <indx_head/errors.hpp>
#include <freertos/binary_semaphore.hpp>

#include <array>
#include <utility>

namespace hal {

void init();
void config_adc_default();
void config_adc_induction_heater();

indx_head::errors::FaultStatusMask get_fault_status();
void set_fault_status(indx_head::errors::FaultStatusMask);
void clear_fault_status(indx_head::errors::FaultStatusMask);
void __attribute__((noreturn)) panic(indx_head::errors::FaultStatusMask);

void set_ind_heater_pwr(bool enabled);

void set_safe_state();

struct FloatReading {
    float value;
    bool valid;
};

namespace i2c {
    // Does everything to prepare and initialize all the I2C peripherals
    void init_comm();

    using fixed = fpm::fixed<int32_t, int64_t, 7>;

    /// Returns ambient and object temperatures
    FloatReading read_tpis_object_temp();
} // namespace i2c

namespace tim {
    void set_printfan_pwm(uint8_t pwm);
    void reset_printfan_rpm_counter();
    uint16_t get_printfan_rpm_counter();

    void set_boardfan_pwm(uint8_t pwm);
    void reset_boardfan_rpm_counter();
    uint16_t get_boardfan_rpm_counter();
} // namespace tim

namespace adc {
    enum class Channel : uint8_t {
        board_temp,
        induction_feadback,
        mcu_temp,
        vref_int,
        input_voltage,
        heater_current,
        _cnt
    };

    namespace impl {
        alignas(uint32_t) extern std::array<uint16_t, std::to_underlying(Channel::_cnt)> buffer;
    }
} // namespace adc

namespace peripherals {
    extern ADC_HandleTypeDef hadc1;
    extern freertos::BinarySemaphore adc_semaphore;
} // namespace peripherals

} // namespace hal

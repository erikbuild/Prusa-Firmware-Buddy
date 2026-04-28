#include "hal.hpp"
#include "critical_section.hpp"
#include "rtt.hpp"
#include <filters/debouncer.hpp>

#include <bsod/bsod.h>
#include <fpm/math.hpp>
#include <lp5817/lp5817.hpp>
#include <freertos/binary_semaphore.hpp>
#include <freertos/mutex.hpp>
#include <freertos/timing.hpp>
#include <raii/lock_guard.hpp>
#include <stm32c0xx_hal.h>

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <span>

namespace hal::peripherals {
extern I2C_HandleTypeDef hi2c1;
}

namespace hal::i2c {
namespace {

    freertos::Mutex i2c_mutex {};
    freertos::BinarySemaphore i2c_it_semaphore {};
    std::atomic<bool> i2c_error_flag { false };
    std::atomic<bool> waiting_for_i2c { false }; // Track if we're waiting for I2C completion

    // Timeout for I2C operations in milliseconds
    static constexpr uint32_t I2C_TIMEOUT_MS = 50;

    /// Attempts to recover I2C bus after an error
    void i2c_recover() {
        rtt::print("i2c: recover\n");
        // Reset I2C peripheral
        __HAL_I2C_DISABLE(&peripherals::hi2c1);
        // Small delay to allow bus to settle
        freertos::delay(1);
        __HAL_I2C_ENABLE(&peripherals::hi2c1);
        i2c_error_flag.store(false);
    }

    namespace thermometer {
        struct EepromData {
            uint8_t lookup = 0;
            uint16_t ptat25 = 0;
            fixed m { 0 };
            uint32_t u0 = 0;
            uint32_t uout1 = 0;
            uint8_t t_obj1 = 0;
            // Could be uint32_t, but to calcualte it we need floats and we would instantly convert it to float anyway
            float k_inv = 0;
        } eeprom_data {};
        static constexpr uint8_t address = 0b000'1100;

        bool do_general_call() {
            LockGuard lg { i2c_mutex };
            // F*CK you HAL I want to make this const(expr)
            static std::array<uint8_t, 2> data = { 0x04, 0x00 }; // General Call Reload
            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);
            if (HAL_I2C_Master_Transmit_IT(&peripherals::hi2c1, 0x00 /* General Call Address */, data.data(), data.size()) != HAL_OK) {
                waiting_for_i2c.store(false);
                i2c_recover();
                return false;
            }
            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, 0x00);
                i2c_recover();
                return false;
            }
            return !i2c_error_flag.load();
        }

        enum class EepromSetting : uint8_t {
            disable = 0x00,
            enable = 0x80,
        };

        bool set_eeprom_reading(EepromSetting setting) {
            LockGuard lg { i2c_mutex };
            uint8_t data = static_cast<uint8_t>(setting);
            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);
            if (HAL_I2C_Mem_Write_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1), 0x1f /* eeprom settings */, I2C_MEMADD_SIZE_8BIT, &data, sizeof(data)) != HAL_OK) {
                waiting_for_i2c.store(false);
                i2c_recover();
                return false;
            }
            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1));
                i2c_recover();
                return false;
            }
            if (i2c_error_flag.load()) {
                i2c_recover();
                return false;
            }
            return true;
        }

        struct SensorData {
            uint32_t tp_object = 0;
            uint16_t tp_ambient = 0;
            bool valid = false;
        };

        SensorData read_sensor_data() {
            LockGuard lg { i2c_mutex };
            SensorData sd {};
            std::array<std::byte, 4> raw_sensor_data {};

            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);

            HAL_StatusTypeDef status = HAL_I2C_Mem_Read_IT(
                &peripherals::hi2c1,
                static_cast<uint16_t>(address << 1),
                0x1,
                I2C_MEMADD_SIZE_8BIT,
                reinterpret_cast<uint8_t *>(raw_sensor_data.data()),
                raw_sensor_data.size());

            if (status != HAL_OK) {
                // Failed to start I2C operation
                waiting_for_i2c.store(false);
                i2c_recover();
                return sd;
            }

            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                // Timeout waiting for I2C - recover and return invalid data
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1));
                i2c_recover();
                return sd;
            }

            if (i2c_error_flag.load()) {
                // I2C error occurred during transfer
                i2c_recover();
                return sd;
            }

            sd.tp_object = (static_cast<uint32_t>(raw_sensor_data.at(0)) << 8 | static_cast<uint32_t>(raw_sensor_data.at(1))) << 1 | static_cast<uint32_t>(raw_sensor_data.at(2) >> 7);
            sd.tp_ambient = (static_cast<uint16_t>(raw_sensor_data.at(2) & std::byte { 0x7f }) << 8) | static_cast<uint16_t>(raw_sensor_data.at(3));
            sd.valid = true;
            return sd;
        }

        constexpr float degC0asKf = 273.15f;
        constexpr fixed degC0asK = fixed(degC0asKf);
        constexpr float degC25asKf = 25.f + degC0asKf;
        constexpr fixed degC25asK = fixed(degC25asKf);

        template <typename T, T min, T max, T step>
        consteval auto generate_lookup_table(T (*func)(T)) {
            constexpr size_t size = static_cast<size_t>((max - min) / step) + 1;
            std::array<T, size> table {};
            for (size_t i = 0; i < size; ++i) {
                table[i] = func(min + static_cast<T>(i) * step);
            }
            return table;
        }

        constexpr fixed calculate_ambient_kelvin(uint32_t tp_ambient) {
            return degC25asK + fixed(static_cast<int32_t>(tp_ambient) - eeprom_data.ptat25) / eeprom_data.m;
        }

        constexpr float f_exp_f = 4.2f;
        constexpr float f(float x) { return std::pow(x, f_exp_f); };
        // This function call is used to calculate f for ambient temp (according to datasheet the range is -25C - 80C, but the value is in Kelvin)
        // But we might go up to 105C in reality - I don't know if it is possible (if the ADC range won't overflow), but let's be safe
        // So let's make the range from -30C to 110C
        constexpr float f_mapped(fixed x) {
            static constexpr float min = degC0asKf - 30.f;
            static constexpr fixed min_fixed = degC0asK - 30;
            static constexpr float max = degC0asKf + 110.f;
            static constexpr fixed max_fixed = degC0asK + 110;
            static constexpr size_t step = 2;
            static constexpr auto lookup_table = generate_lookup_table<float, min, max, float(step)>(f);
            if (x < min_fixed || x > max_fixed) {
                return f(float(x));
            } else {
                const size_t index = static_cast<size_t>((x - min_fixed) / step);
                assert(index + 1 < lookup_table.size()); // Should be always true
                const fixed offset = x - (min_fixed + index * step);
                const float ratio = float(offset) / step;
                const float next_diff = lookup_table.at(index + 1) - lookup_table.at(index);
                return lookup_table.at(index) + next_diff * ratio;
            }
        }

        constexpr float F_exp_f = 1 / f_exp_f;
        constexpr float F(float x) { return std::pow(x, F_exp_f); };

        constexpr bool validate_checksum(std::span<const std::byte, 32> data) {
            auto checksum = static_cast<uint16_t>(data[0]);
            const uint16_t expected_checksum = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[2]);
            for (size_t i = 3; i < data.size(); ++i) {
                checksum += static_cast<uint8_t>(data[i]);
            }
            return checksum == expected_checksum;
        }

        bool initialized = false;

        bool read_eeprom_calibration() {
            LockGuard lg { i2c_mutex };
            std::array<std::byte, 32> raw {};
            static constexpr uint8_t start_addr = 32;
            i2c_error_flag.store(false);
            waiting_for_i2c.store(true);

            if (HAL_I2C_Mem_Read_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1), start_addr, I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t *>(raw.data()), raw.size()) != HAL_OK) {
                waiting_for_i2c.store(false);
                i2c_recover();
                return false;
            }
            if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                waiting_for_i2c.store(false);
                HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, static_cast<uint16_t>(address << 1));
                i2c_recover();
                return false;
            }
            if (i2c_error_flag.load()) {
                i2c_recover();
                return false;
            }

            if (raw.at(0) != std::byte { 0x3 } || !validate_checksum(raw)) {
                return false;
            }

            eeprom_data.lookup = static_cast<uint8_t>(raw.at(9));
            if (eeprom_data.lookup != 2) {
                return false;
            }

            eeprom_data.ptat25 = static_cast<uint16_t>(raw.at(10)) << 8 | static_cast<uint16_t>(raw.at(11));
            const uint16_t raw_m_reg = static_cast<uint16_t>(raw.at(12)) << 8 | static_cast<uint16_t>(raw.at(13));
            eeprom_data.m = fixed(raw_m_reg) / 100;
            const auto raw_u0_reg = static_cast<uint16_t>(raw.at(14)) << 8 | static_cast<uint16_t>(raw.at(15));
            eeprom_data.u0 = raw_u0_reg + 32768;
            const auto raw_uout1_reg = static_cast<uint16_t>(raw.at(16)) << 8 | static_cast<uint16_t>(raw.at(17));
            eeprom_data.uout1 = raw_uout1_reg * 2;
            eeprom_data.t_obj1 = static_cast<uint8_t>(raw.at(18));

            const auto u_div = static_cast<int32_t>(eeprom_data.uout1) - static_cast<int32_t>(eeprom_data.u0);
            // NOTE: Expensive float op, but OK since it is ideally only done once at init (on failed comm it tries reinit every 2s)
            eeprom_data.k_inv = (f(eeprom_data.t_obj1 + degC0asKf) - f(degC25asKf)) / static_cast<float>(u_div) * 1.96f; // Emisivity 0.51
            return true;
        }

        bool init() {
            if (!do_general_call()) {
                return false;
            }
            freertos::delay(2);

            set_eeprom_reading(EepromSetting::enable);
            bool ok = read_eeprom_calibration();
            set_eeprom_reading(EepromSetting::disable);

            initialized = ok;
            return ok;
        }
    } // namespace thermometer

    namespace leds {

        class Impl {
        public:
            [[nodiscard]] bool write_memory(::i2c::Address address, uint8_t offset, std::span<const std::byte> tx_buff) {
                i2c_error_flag.store(false);
                waiting_for_i2c.store(true);
                const auto ret = HAL_I2C_Mem_Write_IT(&peripherals::hi2c1, address << 1, offset, I2C_MEMADD_SIZE_8BIT, const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(tx_buff.data())), tx_buff.size());
                if (ret != HAL_OK) {
                    waiting_for_i2c.store(false);
                    i2c_recover();
                    return false;
                }
                if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                    waiting_for_i2c.store(false);
                    HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, (address << 1));
                    i2c_recover();
                    return false;
                }
                if (i2c_error_flag.load()) {
                    i2c_recover();
                    return false;
                }
                return true;
            }

            [[nodiscard]] bool read_memory(::i2c::Address address, uint8_t offset, std::span<std::byte> rx_buff) {
                i2c_error_flag.store(false);
                waiting_for_i2c.store(true);
                const auto ret = HAL_I2C_Mem_Read_IT(&peripherals::hi2c1, (address << 1), offset, I2C_MEMADD_SIZE_8BIT, reinterpret_cast<uint8_t *>(rx_buff.data()), rx_buff.size());
                if (ret != HAL_OK) {
                    waiting_for_i2c.store(false);
                    i2c_recover();
                    return false;
                }
                if (!i2c_it_semaphore.try_acquire_for(I2C_TIMEOUT_MS)) {
                    waiting_for_i2c.store(false);
                    HAL_I2C_Master_Abort_IT(&peripherals::hi2c1, (address << 1));
                    i2c_recover();
                    return false;
                }
                if (i2c_error_flag.load()) {
                    i2c_recover();
                    return false;
                }
                return true;
            }

            bool raw_transmit([[maybe_unused]] ::i2c::Address address, [[maybe_unused]] size_t offset, [[maybe_unused]] std::span<const std::byte> tx_buf) {
                bsod_unreachable();
            }
            bool raw_receive([[maybe_unused]] ::i2c::Address address, [[maybe_unused]] size_t offset, [[maybe_unused]] std::span<std::byte> rx_buf) {
                bsod_unreachable();
            }

            static void delay_ms(uint32_t ms) {
                freertos::delay(ms);
            }
        };

        using Controller = lp5817::LP5817<Impl>;
        Controller controller;

        void init() {
            LockGuard lg { i2c_mutex };

            if (const auto res = controller.init(); !res.has_value()) {
                rtt::print("i2c: leds init failed");
            }
        }
    } // namespace leds
} // namespace

void init_comm() {
    thermometer::init();
    leds::init();
}

FloatReading read_tpis_object_temp() {
    static float last_valid_object_temperature_celsius = 25.0f; // Default to room temperature

    // A jump > 50°C between consecutive readings is physically impossible
    static constexpr int32_t max_plausible_jump = 50; // °C, integer compare
    // Accept after 3 consecutive implausible readings
    static Debouncer<bool> temp_debouncer { false, 3 };

    if (!thermometer::initialized) {
        // Periodically retry thermometer init (every ~2s, called at 300Hz)
        static constexpr uint32_t REINIT_INTERVAL_MS = 2000;
        static uint32_t last_attempt_ms = 0;
        const uint32_t now = HAL_GetTick();
        if (now - last_attempt_ms >= REINIT_INTERVAL_MS) {
            last_attempt_ms = now;
            if (!thermometer::init()) {
                rtt::print("i2c: thermo init failed\n");
            }
        }
        return {
            .object_temperature_celsius = last_valid_object_temperature_celsius,
            .valid = false
        };
    }
    // FIXME: Use scaled integers
    const auto sensor_data = thermometer::read_sensor_data();
    if (!sensor_data.valid) {
        rtt::print("i2c: thermo read failed\n");
        // Return last valid temperature on error
        return {
            .object_temperature_celsius = last_valid_object_temperature_celsius,
            .valid = false,
        };
    }
    const auto t_ambient_k = thermometer::calculate_ambient_kelvin(sensor_data.tp_ambient);
    const auto val = static_cast<float>(static_cast<int32_t>(sensor_data.tp_object) - static_cast<int32_t>(thermometer::eeprom_data.u0)) * thermometer::eeprom_data.k_inv;
    const float t_obj_k = thermometer::F(val + thermometer::f_mapped(t_ambient_k));
    const float object_temperature_celsius = t_obj_k - thermometer::degC0asKf;

    const int32_t diff = static_cast<int32_t>(object_temperature_celsius) - static_cast<int32_t>(last_valid_object_temperature_celsius);
    const bool plausible = (diff > -max_plausible_jump) && (diff < max_plausible_jump);
    temp_debouncer.push(plausible);

    if (plausible) {
        last_valid_object_temperature_celsius = object_temperature_celsius;
        return {
            .object_temperature_celsius = last_valid_object_temperature_celsius,
            .valid = true,
        };
    }

    if (temp_debouncer.is_stable() && !temp_debouncer.value()) {
        last_valid_object_temperature_celsius = object_temperature_celsius;
        return {
            .object_temperature_celsius = object_temperature_celsius,
            .valid = true,
        };
    }

    // Transient glitch — return last known good value
    return {
        .object_temperature_celsius = last_valid_object_temperature_celsius,
        .valid = true,
    };
}

void set_led_pwm(uint8_t r, uint8_t g, uint8_t b) {
    LockGuard lg { i2c_mutex };

    if (const auto res = leds::controller.set_color(r, g, b); !res.has_value()) {
        rtt::print("i2c: leds set_color_failed\n");
    }
}

void set_led_mode([[maybe_unused]] indx_head::leds::Mode mode) {
    // INDX_HEAD_TODO
}

} // namespace hal::i2c

// TODO: alias these SOBs
extern "C" void HAL_I2C_MasterTxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    using namespace hal::peripherals;
    assert(hi2c == &hi2c1);
    if (hal::i2c::waiting_for_i2c.exchange(false)) {
        hal::i2c::i2c_it_semaphore.release_from_isr();
    }
}

extern "C" void HAL_I2C_MasterRxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    using namespace hal::peripherals;
    assert(hi2c == &hi2c1);
    if (hal::i2c::waiting_for_i2c.exchange(false)) {
        hal::i2c::i2c_it_semaphore.release_from_isr();
    }
}

extern "C" void HAL_I2C_MemTxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    using namespace hal::peripherals;
    assert(hi2c == &hi2c1);
    if (hal::i2c::waiting_for_i2c.exchange(false)) {
        hal::i2c::i2c_it_semaphore.release_from_isr();
    }
}

extern "C" void HAL_I2C_MemRxCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    using namespace hal::peripherals;
    assert(hi2c == &hi2c1);
    if (hal::i2c::waiting_for_i2c.exchange(false)) {
        hal::i2c::i2c_it_semaphore.release_from_isr();
    }
}

extern "C" void HAL_I2C_ErrorCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    using namespace hal::peripherals;
    assert(hi2c == &hi2c1);
    hal::i2c::i2c_error_flag.store(true);
    if (hal::i2c::waiting_for_i2c.exchange(false)) {
        hal::i2c::i2c_it_semaphore.release_from_isr();
    }
}

extern "C" void HAL_I2C_AbortCpltCallback([[maybe_unused]] I2C_HandleTypeDef *hi2c) {
    using namespace hal::peripherals;
    assert(hi2c == &hi2c1);
    // Abort completed - semaphore already released by error callback or will be by recover
}

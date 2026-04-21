#include "app.hpp"

#include "hal.hpp"
#include "heater.hpp"
#include "timing.hpp"
#include <filters/debouncer.hpp>

#include "critical_section.hpp"
#include "watchdog.hpp"

#include <freertos/timing.hpp>

#include <atomic>
#include <algorithm>

namespace {
std::atomic<int16_t> nozzle_temp = 25 * 100; // default 25*C stored in centiDeg - modbus reads before the first valid TPiS reading don't report 0°C,
constexpr float max_nozzle_temp = 330.f;
constexpr float min_nozzle_temp = 5.f;
constexpr uint32_t invalid_nozzle_temp_timeout_ms = 1000 * 2;
/// Target nozzle temperature in DegC
std::atomic<uint16_t> target_temp = 0;
constexpr uint16_t max_target_temp = 300;

std::atomic<indx_head::NozzlePresence> nozzle_present = indx_head::NozzlePresence::unknown;
std::atomic<uint16_t> nozzle_invalidation_ack = 0;

Debouncer<bool> nozzle_debouncer { false, 3 }; // 3 consecutive identical readings to settle

constexpr uint32_t control_frequency = 300 /*Hz*/;
constexpr uint32_t control_delay_us = 1'000'000 / control_frequency;

constexpr uint32_t fan_update_frequency = 10 /*Hz*/;
constexpr uint32_t fan_update_delay_us = 1'000'000 / fan_update_frequency;

std::atomic<uint8_t> printfan_pwm = 0;
std::atomic<uint8_t> boardfan_pwm = 0;

std::atomic<uint32_t> printfan_start_ms = 0;
std::atomic<uint32_t> boardfan_start_ms = 0;

std::atomic<indx_head::leds::LedConfig> leds_config = {};
std::atomic<bool> leds_changed = true;

std::atomic<bool> selftest_mode = false;
} // namespace

namespace app {

void run() {
    hal::i2c::init_comm();

    // Wait for the temperature data to be stable
    freertos::delay(300);

    uint64_t last_induction_control = timing::get_timestamp_us();
    uint64_t last_fan_update = timing::get_timestamp_us();
    uint32_t last_valid_nozzle_temp_ms = timing::get_timestamp_ms(); // Timestamp of last valid nozzle temp reading
    FOREVER_WITH_WATCHDOG(100) {
        const auto now = timing::get_timestamp_us();

        // Induction heater control loop
        if ((now - last_induction_control) >= control_delay_us) {
            last_induction_control = now;
            hal::FloatReading nozzle_temp_reading = hal::i2c::read_tpis_object_temp();

            if (nozzle_temp_reading.valid) {
                if (nozzle_temp_reading.value > max_nozzle_temp) {
                    hal::panic(indx_head::errors::FaultStatusMask::nozzle_max_temp);
                } else if (nozzle_temp_reading.value < min_nozzle_temp) {
                    hal::panic(indx_head::errors::FaultStatusMask::nozzle_min_temp);
                }
                last_valid_nozzle_temp_ms = timing::get_timestamp_ms();
            } else if (timing::get_timestamp_ms() - last_valid_nozzle_temp_ms > invalid_nozzle_temp_timeout_ms) {
                hal::panic(indx_head::errors::FaultStatusMask::tpis_invalid_timeout);
            }

            int16_t nzl_temp = static_cast<int16_t>(nozzle_temp_reading.value * 100.f);
            nozzle_temp.store(nzl_temp);

            inductionHeater.heater_control(target_temp.load() * 100 /*centiDeg*/, nzl_temp);
        }

        // Fans and leds control loop
        if (now - last_fan_update >= fan_update_delay_us) {
            last_fan_update = now;

            // Print fan - direct PWM control
            hal::tim::set_printfan_pwm(printfan_pwm.load());

            // Heatbreak/Board fan
            {
                // Auto mode - thermal loop controls fan
                const bool is_heating = target_temp.load() > 0;
                const bool nozzle_temp_threshold_reached = nozzle_temp.load() > 50 * 100; /*stored in centiDeg*/
                const uint8_t pwm = (is_heating || nozzle_temp_threshold_reached || selftest_mode.load()) ? 255 : 0;
                hal::tim::set_boardfan_pwm(pwm);
                if (boardfan_pwm == 0) {
                    // MUST be before setting the PWM to avoid race conditions
                    boardfan_start_ms = freertos::millis();
                }
                boardfan_pwm = pwm;
            }

            // LEDs control loop
            if (target_temp.load() == 0 && leds_changed) {
                leds_changed = false;
                auto cfg = leds_config.load();
                hal::i2c::set_led_pwm(cfg.r, cfg.g, cfg.b);
                hal::i2c::set_led_mode(cfg.mode);
            }
        }

        freertos::delay(1);
    }
}

int16_t get_nozzle_temp() {
    return nozzle_temp.load();
}

void set_nozzle_present(indx_head::NozzlePresence state) {
    if (state == indx_head::NozzlePresence::unknown) {
        return; // Invalid analysis — do not disturb debounce
    }

    CriticalSection cs; // ISR-safe guard against concurrent invalidate_nozzle_presence() from modbus task
    nozzle_debouncer.push(state == indx_head::NozzlePresence::present);
    if (nozzle_debouncer.is_stable()) {
        nozzle_present.store(nozzle_debouncer.value()
                ? indx_head::NozzlePresence::present
                : indx_head::NozzlePresence::absent);
    }
}

indx_head::NozzlePresence get_nozzle_present() {
    return nozzle_present.load();
}

void invalidate_nozzle_presence(uint16_t ack_value) {
    {
        CriticalSection cs; // ISR-safe guard against concurrent set_nozzle_present() from DMA ISR
        nozzle_debouncer.destabilize();
        nozzle_present.store(indx_head::NozzlePresence::unknown);
    }
    nozzle_invalidation_ack.store(ack_value);
}

uint16_t get_nozzle_invalidation_ack() {
    return nozzle_invalidation_ack.load();
}

void set_nozzle_target_temp(uint16_t new_target) {
    if (new_target > max_target_temp) {
        target_temp.store(max_target_temp);
    } else {
        target_temp.store(new_target);
    }
}

void set_led_config(const indx_head::leds::LedConfig cfg) {
    leds_config.store(cfg);
    leds_changed = true;
}

void set_printfan_pwm(uint8_t pwm) {
    if (printfan_pwm == 0) {
        // MUST be before setting the PWM to avoid race conditions
        printfan_start_ms = freertos::millis();
    }
    printfan_pwm = pwm;
}

uint8_t get_printfan_pwm() {
    return printfan_pwm.load();
}

uint8_t get_boardfan_pwm() {
    return boardfan_pwm.load();
}

static constexpr uint32_t fan_startup_duration_ms = 2000;

bool is_printfan_rpm_ok() {
    return printfan_pwm == 0 || hal::tim::get_printfan_rpm_counter() > 0 || freertos::millis() - printfan_start_ms < fan_startup_duration_ms;
}

bool is_boardfan_rpm_ok() {
    return boardfan_pwm == 0 || hal::tim::get_boardfan_rpm_counter() > 0 || freertos::millis() - boardfan_start_ms < fan_startup_duration_ms;
}

void set_selftest_mode(bool enabled) {
    selftest_mode.store(enabled);
}
} // namespace app

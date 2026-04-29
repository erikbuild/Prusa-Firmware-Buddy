#include "app.hpp"

#include "hal.hpp"
#include "heater.hpp"
#include "timing.hpp"
#include <filters/debouncer.hpp>

#include "critical_section.hpp"
#include "watchdog.hpp"
#include "hotend_temp_compensation.hpp"

#include <freertos/timing.hpp>

#include <atomic>
#include <algorithm>

namespace {

// default 25*C stored in centiDeg - modbus reads before the first valid TPiS reading don't report 0°C,
std::atomic<int16_t> nozzle_temp_uncompensated_c100 = 25 * 100;
std::atomic<int16_t> nozzle_temp_compensated_c100 = 25 * 100;
std::atomic<int16_t> tpis_ambient_temp_c100 = 25 * 100;

/// See indx_head::modbus::Status::hotend_temp_raw_c100_dt_s
std::atomic<int16_t> hotend_temp_raw_c100_dt_s = 0;
bool can_calculate_nozzle_temp_slope = false;

constexpr float max_nozzle_temp = 330.f;
constexpr float min_nozzle_temp = 5.f;
constexpr float max_tpis_ambient_temp = 85.f;
constexpr float min_tpis_ambient_temp = 10.f;
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
std::atomic<uint8_t> heatbreak_fan_pwm = 0;

std::atomic<uint32_t> printfan_start_ms = 0;
std::atomic<uint32_t> heatbreak_fan_start_ms = 0;

std::atomic<indx_head::leds::LedConfig> leds_config = {};
std::atomic<bool> leds_changed = true;

std::atomic<bool> selftest_mode = false;

/// Integrates (hotend duty cycle 0-1)^2 over time - in us units
/// Overflows are expected
std::atomic<uint32_t> hotend_duty_cycle_sq_integral_us { 0 };

int16_t validate_board_temperature() {
    constexpr int16_t min_board_temp_degC = 10;
    constexpr int16_t max_board_temp_degC = 95;
    const int16_t board_temp_degC = hal::adc::get_board_temp();
    if (board_temp_degC < min_board_temp_degC) {
        hal::panic(indx_head::errors::FaultStatusMask::board_min_temp);
    } else if (board_temp_degC > max_board_temp_degC) {
        hal::panic(indx_head::errors::FaultStatusMask::board_max_temp);
    }
    return board_temp_degC;
}

} // namespace

namespace app {

void run() {
    hal::i2c::init_comm();

    // Wait for the temperature data to be stable
    freertos::delay(300);

    uint64_t last_induction_control_us = timing::get_timestamp_us();
    uint64_t last_fan_update_us = timing::get_timestamp_us();
    uint32_t last_valid_nozzle_temp_ms = timing::get_timestamp_ms(); // Timestamp of last valid nozzle temp reading

    FOREVER_WITH_WATCHDOG(100) {
        const uint64_t now_us = timing::get_timestamp_us();
        const uint32_t now_ms = timing::get_timestamp_ms();

        // Induction heater control loop
        const uint64_t induction_control_dt_us = now_us - last_induction_control_us;
        if (induction_control_dt_us >= control_delay_us) {
            hotend_temp_compensation::step();

            last_induction_control_us = now_us;
            hal::TemperatureReading nozzle_temp_reading = hal::i2c::read_tpis_temperature();

            if (nozzle_temp_reading.valid) {
                if (nozzle_temp_reading.object_temperature_celsius > max_nozzle_temp) {
                    hal::panic(indx_head::errors::FaultStatusMask::nozzle_max_temp);
                } else if (nozzle_temp_reading.object_temperature_celsius < min_nozzle_temp) {
                    hal::panic(indx_head::errors::FaultStatusMask::nozzle_min_temp);
                }

                if (nozzle_temp_reading.ambient_temperature_celsius > max_tpis_ambient_temp) {
                    hal::panic(indx_head::errors::FaultStatusMask::tpis_ambient_max_temp);
                } else if (nozzle_temp_reading.ambient_temperature_celsius < min_tpis_ambient_temp) {
                    hal::panic(indx_head::errors::FaultStatusMask::tpis_ambient_min_temp);
                }

                last_valid_nozzle_temp_ms = now_ms;

            } else if (now_ms - last_valid_nozzle_temp_ms > invalid_nozzle_temp_timeout_ms) {
                hal::panic(indx_head::errors::FaultStatusMask::tpis_invalid_timeout);
            }

            // Note: If !nozzle_temp_reading.valid, nozzle_temp_reading contains last valid value

            const int16_t nozzle_temp_uncompensated_c100 = static_cast<int16_t>(nozzle_temp_reading.object_temperature_celsius * 100.f);
            const int16_t nozzle_temp_compensated_c100 = nozzle_temp_uncompensated_c100 - hotend_temp_compensation::get_current_compensation_c100();

            // Calculate slope
            if (can_calculate_nozzle_temp_slope) {
                const int64_t uncompensated_raw_slope = int64_t(nozzle_temp_uncompensated_c100 - ::nozzle_temp_uncompensated_c100.load()) * 1000000 / int64_t(induction_control_dt_us);

                // Clamp to sane values to prevent too big spikes
                const int32_t clamped_raw_slope = int32_t(std::clamp<int64_t>(uncompensated_raw_slope, -100 * 100, 100 * 100));

                // Apply exponential fadeout to average the values a bit
                const int16_t filtered_slope = int16_t((int32_t(hotend_temp_raw_c100_dt_s.load()) * 7 + clamped_raw_slope * 1) / 8);

                ::hotend_temp_raw_c100_dt_s.store(filtered_slope);
            }

            ::nozzle_temp_uncompensated_c100.store(nozzle_temp_uncompensated_c100);
            ::nozzle_temp_compensated_c100.store(nozzle_temp_compensated_c100);
            ::tpis_ambient_temp_c100.store(static_cast<int16_t>(nozzle_temp_reading.ambient_temperature_celsius * 100.f));

            // Start calculating slope only after the nozzle_temps store actual readouts
            // to prevent a slope spike on first valid readout
            // Note: If !nozzle_temp_reading.valid, nozzle_temp_reading contains last valid value
            can_calculate_nozzle_temp_slope |= nozzle_temp_reading.valid;

            inductionHeater.heater_control(target_temp.load() * 100 /*centiDeg*/, nozzle_temp_compensated_c100);

            // Integrate duty cycle
            hotend_duty_cycle_sq_integral_us += uint32_t(inductionHeater.current_duty_cycle_sq() * induction_control_dt_us);

            // Just to give scope what numbers we're dealing with
            // Duty cycle is 0-1, control_delay_us is in thousands - so when we cast to uint32_t after the duty cycle multiplication, we should get reasonably precise values
            static_assert(control_delay_us >= 1000 && control_delay_us <= 10000);
        }

        // Fans and leds control loop
        if (now_us - last_fan_update_us >= fan_update_delay_us) {
            last_fan_update_us = now_us;

            // Print fan - direct PWM control
            hal::tim::set_printfan_pwm(printfan_pwm.load());

            const int16_t board_temp_degC = validate_board_temperature();
            // Heatbreak fan
            {
                // Auto mode - thermal loop controls fan
                const bool is_heating = target_temp.load() > 0;
                const bool nozzle_temp_threshold_reached = get_nozzle_temp_compensated_c100() > 50 * 100; /*stored in centiDeg*/
                const bool board_temp_threshold_reached = board_temp_degC > 50; /*stored in Deg*/
                const uint8_t pwm = (is_heating || nozzle_temp_threshold_reached || board_temp_threshold_reached || selftest_mode.load()) ? 255 : 0;
                hal::tim::set_heatbreak_fan_pwm(pwm);
                if (heatbreak_fan_pwm == 0) {
                    // MUST be before setting the PWM to avoid race conditions
                    heatbreak_fan_start_ms = freertos::millis();
                }
                heatbreak_fan_pwm = pwm;
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

int16_t get_nozzle_temp_uncompensated_c100() {
    return nozzle_temp_uncompensated_c100.load();
}

int16_t get_nozzle_temp_compensated_c100() {
    return nozzle_temp_compensated_c100.load();
}

int16_t get_hotend_temp_raw_c100_dt_s() {
    return hotend_temp_raw_c100_dt_s.load();
}

uint32_t get_hotend_duty_cycle_sq_integral_us() {
    return hotend_duty_cycle_sq_integral_us.load();
}

int16_t get_tpis_ambient_temp_c100() {
    return tpis_ambient_temp_c100.load();
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
    // INDX_TODO: The RPM measuring doesn't work for PWM below ~90%. We do want
    // to check the RPM at least when PWM is close to 100%, but at the point of
    // a new, higher PWM being set the RPM may have not caught up, hence we use
    // this timer any time we're increasing PWM as a grace period before
    // starting to check the RPM.
    // The original condition was:
    // if (printfan_pwm == 0) {
    if (pwm > printfan_pwm) {
        // MUST be before setting the PWM to avoid race conditions
        printfan_start_ms = freertos::millis();
    }
    printfan_pwm = pwm;
}

uint8_t get_printfan_pwm() {
    return printfan_pwm.load();
}

uint8_t get_heatbreak_fan_pwm() {
    return heatbreak_fan_pwm.load();
}

static constexpr uint32_t fan_startup_duration_ms = 2000;

bool is_printfan_rpm_ok() {
    // TODO Workaround: RPM measurement doesn't work when PWM is off (need to
    // measure in the periods when PWM is high). Until this is fixed, consider
    // the fan always OK if PWM is not high enough. After RPM measurement is
    // fixed, the printfan_pwm check below should be printfan_pwm == 0.
    return printfan_pwm < 250 || hal::tim::get_printfan_rpm_counter() > 0 || freertos::millis() - printfan_start_ms < fan_startup_duration_ms;
}

bool is_heatbreak_fan_rpm_ok() {
    return heatbreak_fan_pwm == 0 || hal::tim::get_heatbreak_fan_rpm_counter() > 0 || freertos::millis() - heatbreak_fan_start_ms < fan_startup_duration_ms;
}

void set_selftest_mode(bool enabled) {
    selftest_mode.store(enabled);
}
} // namespace app

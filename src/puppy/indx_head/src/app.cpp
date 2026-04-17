#include "app.hpp"

#include "hal.hpp"
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

constexpr uint32_t control_frequency = 300 /*Hz*/;
constexpr uint32_t control_delay_us = 1'000'000 / control_frequency;
} // namespace

namespace app {

void run() {
    hal::i2c::init_comm();

    // Wait for the temperature data to be stable
    freertos::delay(300);

    uint64_t last_induction_control = timing::get_timestamp_us();
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

            // TODO: Update induction heating
        }

        freertos::delay(1);
    }
}

int16_t get_nozzle_temp() {
    return nozzle_temp.load();
}

} // namespace app

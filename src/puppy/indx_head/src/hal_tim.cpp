#include "hal.hpp"

#include <stm32c0xx_hal.h>

#include <atomic>

namespace hal::peripherals {
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
} // namespace hal::peripherals

namespace hal::tim {

namespace {
    // Timer3 configuration:
    // - System clock: 48MHz
    // - Prescaler: 479 (48MHz / 480 = 100kHz timer clock)
    // - Period: 49999 (counter runs 0 to 49999, then wraps)
    // - ICPrescaler: DIV2 (capture every 2nd edge)
    //
    // With 2 pulses/revolution fan and DIV2 prescaler:
    // - Capture occurs once per revolution
    // - Period in timer ticks = time for one revolution
    // - RPM = 60 * timer_freq / period = 60 * 100000 / period = 6'000'000 / period
    constexpr uint32_t timer_period = 50'000;
    constexpr uint32_t rpm_conversion_factor = 6'000'000;

    // Minimum period to consider valid (prevents divide by very small numbers)
    // 6'000'000 / 600 = 20'000 RPM max
    constexpr uint16_t min_period = 300;

    // Maximum period to consider valid (prevents very slow/stuck readings)
    // 6'000'000 / 30'000 = 200 RPM min
    constexpr uint16_t max_period = 30'000;

    // Timeout in timer periods (at 2Hz = 500ms per period, 2 periods = 1 second timeout)
    constexpr uint8_t timeout_periods = 2;

    // State for each channel
    struct ChannelState {
        std::atomic<uint16_t> last_capture { 0 };
        std::atomic<uint16_t> period { 0 };
        std::atomic<uint8_t> timeout_counter { timeout_periods }; // Start timed out (no data yet)
    };

    // Check if period is within valid range
    bool is_period_valid(uint16_t period) {
        return period >= min_period && period <= max_period;
    }

    ChannelState printfan_state;
    ChannelState boardfan_state;

    uint16_t period_to_rpm(uint16_t period, uint8_t timeout) {
        if (timeout >= timeout_periods || !is_period_valid(period)) {
            return 0; // Fan stopped or invalid reading
        }
        return static_cast<uint16_t>(rpm_conversion_factor / period);
    }
} // namespace

void set_printfan_pwm(uint8_t pwm) {
    using namespace peripherals;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, pwm);
}

void reset_printfan_rpm_counter() {
    printfan_state.period.store(0);
    printfan_state.timeout_counter.store(timeout_periods);
}

uint16_t get_printfan_rpm_counter() {
    return period_to_rpm(printfan_state.period.load(), printfan_state.timeout_counter.load());
}

void set_boardfan_pwm(uint8_t pwm) {
    using namespace peripherals;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm);
}

void reset_boardfan_rpm_counter() {
    boardfan_state.period.store(0);
    boardfan_state.timeout_counter.store(timeout_periods);
}

uint16_t get_boardfan_rpm_counter() {
    return period_to_rpm(boardfan_state.period.load(), boardfan_state.timeout_counter.load());
}

// Called from TIM3 update interrupt (timer overflow) to detect fan stopped
void handle_timer_overflow() {
    // Increment timeout counters (saturate at timeout_periods)
    uint8_t pf_timeout = printfan_state.timeout_counter.load();
    if (pf_timeout < timeout_periods) {
        printfan_state.timeout_counter.store(pf_timeout + 1);
    }

    uint8_t bf_timeout = boardfan_state.timeout_counter.load();
    if (bf_timeout < timeout_periods) {
        boardfan_state.timeout_counter.store(bf_timeout + 1);
    }
}

// Called from TIM3 input capture interrupt
void handle_input_capture(uint32_t channel) {
    uint16_t current_capture;
    ChannelState *state;

    if (channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        current_capture = HAL_TIM_ReadCapturedValue(&peripherals::htim3, TIM_CHANNEL_1);
        state = &printfan_state;
    } else if (channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        current_capture = HAL_TIM_ReadCapturedValue(&peripherals::htim3, TIM_CHANNEL_2);
        state = &boardfan_state;
    } else {
        return;
    }

    uint16_t last = state->last_capture.load();
    state->last_capture.store(current_capture);

    // Calculate period with wrap-around handling
    uint16_t period;
    if (current_capture >= last) {
        period = current_capture - last;
    } else {
        // Counter wrapped around
        period = (timer_period - last) + current_capture;
    }

    // Validate period and update if reasonable
    if (is_period_valid(period)) {
        // Only update stored period if we had a valid previous capture
        if (state->timeout_counter.load() < timeout_periods) {
            state->period.store(period);
        }
        // Reset timeout counter on valid capture
        state->timeout_counter.store(0);
    }
    // Invalid periods (noise) are ignored - don't reset timeout
}

} // namespace hal::tim

// HAL callback for input capture events
extern "C" void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim == &hal::peripherals::htim3) {
        hal::tim::handle_input_capture(htim->Channel);
    }
}

// HAL callback for timer period elapsed (overflow)
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &hal::peripherals::htim3) {
        hal::tim::handle_timer_overflow();
    }
}

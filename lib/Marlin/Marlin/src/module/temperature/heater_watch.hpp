/// @file
#pragma once

#include <cstdint>

#include <core/millis_t.h>
#include <wiring_time.h>
#include <module/temperature/temp_defines.hpp>
#include <error_codes.hpp>

/// Sanity-checks that a heater makes progress toward its target temperature.
///
/// arm() records the target as intent; the baseline is captured on the
/// next update() call. This decouples target arming from baseline data,
/// so arming can happen synchronously (e.g. from start_heating()) before
/// the next manage tick refreshes the current temperature.
class HeaterWatch {
public:
    struct Config {
        /// By how many degrees must the temperature increase
        /// within the @p period
        /// for the watch to not trigger
        int16_t temp_increase;

        /// The period for the temp to increase, in seconds
        uint16_t period_s;

        /// Minimum target-current temperature difference for the watch to engage
        int16_t min_temp_diff;

        /// Error code to raise if the watch fails
        ErrCode error_code;

        /// Inverts the checking logic
        /// !!! If true, temp_increase and min_temp_diff should probably be negative
        bool watch_cooling_instead = false;
    };

    explicit HeaterWatch(const Config &config)
        : config_(config) {}

    /// Set the watch target. Baseline is captured by the next update().
    /// target_temp <= 0 disarms.
    void arm(int16_t target_temp);

    /// Tick: captures baseline when pending, fires fatal_error if the period
    /// elapses without sufficient progress.
    void update(float current_temp);

    /// @returns true when armed (pending or watching).
    bool is_running() const {
        return state_ != State::disarmed;
    }

private:
    enum class State : uint8_t {
        disarmed,
        pending,
        watching,
    };

    const Config config_;
    State state_ = State::disarmed;
    int16_t target_temp_ = 0;
    int16_t baseline_threshold_ = 0;
    millis_t next_check_ms_ = 0;
};

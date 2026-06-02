/// @file
#pragma once

#include <cstdint>

namespace utils {

/// Latches "triggered" once the input stays active continuously for
/// `threshold` ticks, and clears as soon as the input becomes inactive.
///
/// The stored timestamp is only used while pending; a triggered latch is
/// therefore unaffected by timestamp wraparound.
class LatchingDebouncer {
public:
    /// Feeds one observation. `input_active` is true when the watched
    /// condition is currently observed; `threshold` is the time the input
    /// must stay continuously active before latching.
    void update(bool input_active, uint32_t now, uint32_t threshold) {
        switch (state_) {
        case State::idle:
            if (input_active) {
                state_ = State::pending;
                pending_since_ = now;
            }
            break;

        case State::pending:
            if (!input_active) {
                state_ = State::idle;
            } else if (now - pending_since_ >= threshold) {
                state_ = State::triggered;
            }
            break;

        case State::triggered:
            if (!input_active) {
                state_ = State::idle;
            }
            break;
        }
    }

    /// Returns whether the debouncer has latched to the triggered state.
    [[nodiscard]] bool is_triggered() const {
        return state_ == State::triggered;
    }

private:
    enum class State { idle,
        pending,
        triggered,
    };

    State state_ = State::idle;
    uint32_t pending_since_ = 0; // meaningful only while state_ == pending
};

} // namespace utils

/// \file
#pragma once

#include <algorithm>
#include <type_traits>
#include <limits>

/// Utility class for making sure that something is not run too often
template <typename T, T max_value = std::numeric_limits<T>::max()>
class RateLimiter {

public:
    // !!! Important! std::convertible_to<T> required to disable automatic type inferration from the min_delay type
    explicit RateLimiter(std::convertible_to<T> auto min_delay)
        : min_delay_(min_delay) {}

    /// Forget any previous events. Next event will not be limited
    void reset() {
        last_event_ = 0;
    }

    /// \returns true if we can perform an action (and also marks the current time as the last event)
    [[nodiscard]] bool check(T now) {
        T diff = 0;

        if (std::is_signed_v<T> && now < last_event_) {
            diff = max_value - last_event_ + now;
        } else {
            diff = static_cast<T>(now - last_event_);
        }

        if (diff < min_delay_ && last_event_ != 0) {
            return false;
        }

        last_event_ = now;
        return true;
    }

    /// \returns how much time is remaining till we can run the event again
    [[nodiscard]] T remaining_cooldown(T now) const {
        if (last_event_ == 0) {
            return 0;
        }

        T diff = 0;

        if (std::is_signed_v<T> && now < last_event_) {
            diff = max_value - last_event_ + now;
        } else {
            diff = static_cast<T>(now - last_event_);
        }

        return min_delay_ - std::min<T>(diff, min_delay_);
    }

private:
    /// Minimum delay between two events
    T min_delay_;

    /// Timestamp of the last event; 0 = no event happened
    T last_event_ = 0;
};

/// @file
#pragma once

#include <cstdint>

#include <utils/uncopyable.hpp>

/// Class that updates the result only after `interval` consecutive pushes provide the same value
template <typename Value_, typename Count_ = uint8_t>
class Debouncer : public Uncopyable {

public:
    using Value = Value_;
    using Count = Count_;

public:
    explicit Debouncer(const Value &initial_value, Count required_stability)
        : stable_value_(initial_value)
        , last_value_(initial_value)
        , required_stability_(required_stability) {}

    void push(const Value &value) {
        if (value != last_value_) {
            last_value_ = value;
            stability_ = 0;
        }

        if (stability_ < required_stability_) {
            stability_++;

            if (stability_ == required_stability_) {
                stable_value_ = value;
            }
        }
    }

    void destabilize() {
        stability_ = 0;
    }

    const Value &value() const {
        return stable_value_;
    }

    bool is_stable() const {
        return stability_ == required_stability_;
    }

private:
    /// Last value the debouncer stabilized on
    Value stable_value_;

    /// Last value provided to the debouncer
    Value last_value_;

    /// How many consecutive times has the debounce been called with the same value
    Count stability_ = 0;

    const Count required_stability_;
};

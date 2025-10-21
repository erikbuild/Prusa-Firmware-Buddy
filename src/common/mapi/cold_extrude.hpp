/// @file
#pragma once

#include <feature/safety_timer/safety_timer.hpp>
#include <module/temperature.h>

namespace mapi {

/// Guard that allows cold extrusion for the duration of its existence
class ColdExtrudeGuard {

public:
    ColdExtrudeGuard()
        : prev_allow_cold_extrude_(thermalManager.allow_cold_extrude) {
        thermalManager.allow_cold_extrude = true;
    }
    ~ColdExtrudeGuard() {
        thermalManager.allow_cold_extrude = prev_allow_cold_extrude_;
    }

private:
    /// Do not wait for the target temperatures to be restored for moves issued in this guard
    buddy::SafetyTimerNonBlockingGuard non_blocking_safety_;

    const bool prev_allow_cold_extrude_;
};

} // namespace mapi

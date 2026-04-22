/// @file
#pragma once

#include <cstdint>

#include "indx_hotend_temp_compensation.hpp"

#include <utils/timing/rate_limiter.hpp>

// The thermometer on the head is basically not close enough to the melt zone
// and there is a significant difference between the measured temperature and what temp the filament is actually on.
// To compensate for this, we calculate a rudimentary filament model on the motherboard
// and send a compensation parameter over the modbus to the head.
// BFW-8630
namespace buddy::indx_hotend_temp_compensation {

/// Wrapper over the platform-invariant compensator that wraps ::indx_hotend_temp_compensation::HotendTempCompensator
/// Works directly on the INDX-head, going around the IndxHotend. This makes sense to be a singleton.
/// !!! To be accessed from marlin thread only
class TempCompensator {

public:
    /// To be called in regular intervals from marlin thread
    void step();

    /// Resets the compensator internal state
    /// To be called on toolchanges or when filament parameters change in general
    void reset_state();

private:
    ::indx_hotend_temp_compensation::HotendTempCompensator compensator_;

    RateLimiter<uint32_t> step_limiter_ms_ { 100 };

    // These don't need to be initialized, they get initialized before is_initialized_ goes to true
    int32_t last_e_steps_;
    float retracted_distance_mm_;
    FilamentType last_filament_;

    bool is_initialized_ : 1 = false;
};

/// Singleton
/// !!! To be accessed from marlin thread only
TempCompensator &temp_compensator();

} // namespace buddy::indx_hotend_temp_compensation

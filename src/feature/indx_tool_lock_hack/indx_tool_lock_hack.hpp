/// @file
#pragma once

#include <utils/badge.hpp>

class PrusaToolChanger;
class Planner;

namespace buddy {

/// INDX head has this annoying feature that it needs to extrude 2 mm to fully finish docking
/// If one would try to retract after picking a tool without extruding first, the tool would fall off
/// This class exists to handle this situation - it will insert a small extrude-deretract
/// To be only called from the marlin thread
class INDXToolLockHack {

public:
    INDXToolLockHack();

    bool is_armed() const {
        return extrusion_needed_mm_ > 0;
    }

    /// Arms the hack; to be called after tool pickup
    void rearm(Badge<PrusaToolChanger>) {
        rearm();
    }

    /// Tracks the extruder move and possibly injects the hack
    void track_extruder_move(float delta_e, Badge<Planner>);

private:
    void rearm();

private:
    /// Remaining distance to extrude before the hack becomes disarmed
    float extrusion_needed_mm_ = 0;
};

INDXToolLockHack &indx_tool_lock_hack();

}; // namespace buddy

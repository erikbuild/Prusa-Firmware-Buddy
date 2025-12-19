#include "filament_tracker.hpp"

#include <algorithm>
#include <cmath>

using namespace buddy;

FilamentTracker::FilamentTracker() {
    for (auto &dist : retracted_distances) {
        // Starts as if it is fully retracted (to the edge of the extruder)
        // After the value validates (travels at least +extruder_to_nozzle_distance) we can start to keep track of retracted distance
        dist = extruder_to_nozzle_distance;
    }
}

void FilamentTracker::track_extruder_move(VirtualToolIndex virtual_tool, float e_delta) {
    const PhysicalToolIndex physical_tool = virtual_tool.to_physical();

    const float new_retracted_distance_unclamped = retracted_distances[physical_tool] - /* inverting to positive */ e_delta;

    // Update retracted distance
    {
        const float new_retraction_distance = std::clamp(new_retracted_distance_unclamped, 0.0f, extruder_to_nozzle_distance);
        retracted_distances[physical_tool] = new_retraction_distance;

        if (!distance_valid.test(physical_tool.to_raw()) && new_retraction_distance == 0.0f) {
            // retracted_distance resets to extruder_to_nozzle_distance and we validate when it reaches zero - we are sure filament is fully in the nozzle
            distance_valid.set(physical_tool.to_raw(), true);
        } else if (distance_valid.test(physical_tool.to_raw()) && new_retraction_distance == extruder_to_nozzle_distance) {
            // if we retract more than extruder_to_nozzle_distance, we most likely lost the track of the retractions, because the filament is no longer engaged with the extruder
            distance_valid.set(physical_tool.to_raw(), false);
        }
    }

    // Update extruded distance
    {
        auto &edist = extruded_distances[virtual_tool];
        float integral;
        edist.fractional += std::max<float>(-new_retracted_distance_unclamped, 0);
        edist.fractional = std::modf(edist.fractional, &integral);
        edist.integral += static_cast<uint32_t>(integral);
    }
}

std::optional<float> FilamentTracker::get_retracted_distance(PhysicalToolIndex physical_tool) const {
    if (!distance_valid.test(physical_tool.to_raw())) {
        return std::nullopt;
    }
    return retracted_distances[physical_tool];
}

uint32_t buddy::FilamentTracker::get_extruded_distance(VirtualToolIndex virtual_tool) const {
    const auto &ed = extruded_distances[virtual_tool];
    return ed.integral;
}

FilamentTracker &buddy::filament_tracker() {
    static FilamentTracker instance;
    return instance;
}

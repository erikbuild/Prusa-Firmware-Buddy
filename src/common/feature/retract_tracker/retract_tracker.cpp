#include "retract_tracker.hpp"

using namespace buddy;

RetractTracker::RetractTracker() {
    reset();
}

void RetractTracker::track_extruder_move(PhysicalToolIndex physical_tool, float extrusion_distance) {
    const float new_retraction_distance = std::clamp(retracted_distances[physical_tool] - /* inverting to positive */ extrusion_distance, 0.0f, extruder_to_nozzle_distance);
    retracted_distances[physical_tool] = new_retraction_distance;

    if (!distance_valid.test(physical_tool.to_raw()) && new_retraction_distance == 0.0f) {
        // retracted_distance resets to extruder_to_nozzle_distance and we validate when it reaches zero - we are sure filament is fully in the nozzle
        distance_valid.set(physical_tool.to_raw(), true);
    } else if (distance_valid.test(physical_tool.to_raw()) && new_retraction_distance == extruder_to_nozzle_distance) {
        // if we retract more than extruder_to_nozzle_distance, we most likely lost the track of the retractions, because the filament is no longer engaged with the extruder
        distance_valid.set(physical_tool.to_raw(), false);
    }
}

void RetractTracker::reset() {
    for (PhysicalToolIndex physical_tool : PhysicalToolIndex::all()) {
        // Starts as if it is fully retracted (to the edge of the extruder)
        // After the value validates (travels at least +extruder_to_nozzle_distance) we can start to keep track of retracted distance
        retracted_distances[physical_tool] = extruder_to_nozzle_distance;
    }
    distance_valid.reset();
}

std::optional<float> RetractTracker::get_retracted_distance(PhysicalToolIndex physical_tool) const {
    if (!distance_valid.test(physical_tool.to_raw())) {
        return std::nullopt;
    }
    return retracted_distances[physical_tool];
}

RetractTracker &buddy::retract_tracker() {
    static RetractTracker instance;
    return instance;
}

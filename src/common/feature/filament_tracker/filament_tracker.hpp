/// \file

#pragma once

#include <utils/uncopyable.hpp>
#include <bitset>
#include <optional>
#include <option/has_nextruder.h>
#include <tool_index.hpp>
#include <utils/storage/strong_index_array.hpp>

namespace buddy {

/**
 *  Passive observer that keeps track of retracted distance on each hotend during printing
 *
 *  On XL, retracting and ramming is handled by PrusaSlicer, which invalidates filament_retracted_distances in persistent storage
 *  and therefore collides with auto_retract feature. This tracker keeps track of the retracted distance on each hotend during printing
 *  and on print finish/abort/stop it saves the values of respected hotends in the persistent storage
 *
 *  Retracted distances are saved in range < 0 ; 254 > (mm)
 *  Value 255 is reserved as invalid / unknown value
 */
class FilamentTracker : Uncopyable {
    friend FilamentTracker &filament_tracker();

public:
#if HAS_NEXTRUDER()
    static constexpr float extruder_to_nozzle_distance = 40.f; // mm
#else
    #error
#endif

    /// Adds \param extrusion_distance (retraction is negative) to temporary value of active hotend
    void track_extruder_move(PhysicalToolIndex physical_tool, float extrusion_distance);

    /// Return temporary value (retraction is positive) of \param physical_tool
    std::optional<float> get_retracted_distance(PhysicalToolIndex physical_tool) const;

private:
    FilamentTracker();
    StrongIndexArray<float, PhysicalToolIndex::count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static> retracted_distances;
    std::bitset<PhysicalToolIndex::count> distance_valid; ///< The hotend validates  by traveling at least +extruder_to_nozzle_distance
};

FilamentTracker &filament_tracker();

} // namespace buddy

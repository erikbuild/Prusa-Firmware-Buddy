#pragma once

#include <utils/uncopyable.hpp>
#include <bitset>
#include <optional>
#include <option/has_nextruder.h>
#include <tool_index.hpp>
#include <utils/storage/strong_index_array.hpp>

namespace buddy {

struct FilamentTracker {

    StrongIndexArray<uint32_t, VirtualToolIndex::count, VirtualToolIndex, VirtualToolIndex::to_raw_static> extruded_distances;

    uint32_t get_extruded_distance(VirtualToolIndex virtual_tool) const {
        return extruded_distances[virtual_tool];
    }
};

inline FilamentTracker &filament_tracker() {
    static FilamentTracker instance;
    return instance;
}

} // namespace buddy

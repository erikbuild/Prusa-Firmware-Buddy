#pragma once

#include <cstdint>
#include <core/types.h>

namespace tool_offset {

struct ProbingConfig {
    xyz_pos_t sensor_position;
    float safe_z_height; // Height above the sensor to safely move in XY
    float sensing_z; // Height above the sensor to actually perform the measurement
    float sensing_diameter; // Diameter of the sensing area
    float sensing_speed_slow;
    float sensing_speed_fast;
    float sweep_rest_time; // Pause between sweep passes (seconds)
    float max_safe_temp; // Maximum nozzle temperature allowed for probing
};

ProbingConfig get_default_probing_config();

} // namespace tool_offset

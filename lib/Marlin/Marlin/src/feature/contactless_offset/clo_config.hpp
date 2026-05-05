#pragma once

#include <core/types.h>
#include <config.h>
#include <printers.h>

namespace tool_offset {

inline constexpr xy_pos_t default_sensor_position {
#if PRINTER_IS_PRUSA_COREONE()
    // Y CAD position is 197.5mm from the homing position, Y homing position is Y_MAX_PRINT_POS
    // X CAD position can be used directly, X homing position is 0
    {
        { 257.f, Y_MAX_PRINT_POS - 197.5f }
    }
#elif PRINTER_IS_PRUSA_COREONEL()
    // So far only copy from COREONE INDX
    // TODO update values for Core ONEL INDX once the values are known
    {
        { 257.f, Y_MAX_PRINT_POS - 197.5f }
    }
#else
    #error "No default probing config for this printer"
#endif
};

struct ProbingConfig {
    xyz_pos_t sensor_position;
    float safe_z_height; // Height above the sensor to safely move in XY
    float sensing_z; // Height above the sensor to actually perform the measurement
    float sensing_distance_x; // Sensing area in X direction
    float sensing_distance_y; // Sensing area in Y direction
    float sensing_speed_slow;
    float sensing_speed_fast;
    float sweep_rest_time; // Pause between sweep passes (seconds)
    float max_safe_temp; // Maximum nozzle temperature allowed for probing
    float symmetry_trim_fraction; // Per-pass second-correlation: keep this central fraction
                                  // (around the first-pass symmetry axis) and re-correlate.
                                  // 1.0 disables, 0.5 keeps central 50%.
    float y_shift_z_probe_offset_from_sensor; // Y shift of the Z probing point from the sensor position, to move it out of the coil area
    static constexpr float sensor_position_update_threshold = 0.2f;
    static constexpr float sensor_position_error_threshold = 3.0f;
};

ProbingConfig get_default_probing_config();

} // namespace tool_offset

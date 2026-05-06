/// @file
/// @brief Tool offset calibration (Z-offset via probing + XY-offset with tool_offset board)
#pragma once

#include <feature/contactless_offset/contactless_offset.hpp>

namespace tool_offset_calibration {

/// Run the full tool offset calibration sequence:
/// 1. Z-offset calibration: probe a line with first tool at reference positions, interpolate other tools
/// 2. XY-offset calibration: for each tool, move to a location of tool_offset board, execute calibration moves, return
/// @param r_param (up to) millimeters of random jitter on X & Y axis during Z_probing each tool
/// @param probe_count number of Z probe repetitions per point to average
/// @return true if calibration was successful
bool run(uint8_t r_param, uint8_t probe_count);

/// Run XY offset calibration for a single tool, without touching Z offset or other tools.
/// @param tool The tool to calibrate
/// @param config The probing configuration to use
/// @return true if calibration was successful
bool calibrate_xy_offset(PhysicalToolIndex tool, const tool_offset::ProbingConfig &config);

/// Overwrite the sensor position in `config` with the calibrated value from the
/// config store, unless that value differs from the default by more than
/// `sensor_position_error_threshold` (in which case the default is kept and an
/// error is logged).
void apply_stored_sensor_position(tool_offset::ProbingConfig &config);

} // namespace tool_offset_calibration

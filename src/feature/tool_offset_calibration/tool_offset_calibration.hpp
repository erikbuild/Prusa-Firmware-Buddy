/// @file
/// @brief Tool offset calibration (Z-offset via probing + XY-offset with tool_offset board)
#pragma once

namespace tool_offset_calibration {

/// Run the full tool offset calibration sequence:
/// 1. Z-offset calibration: probe a line with first tool at reference positions, interpolate other tools
/// 2. XY-offset calibration: for each tool, move to a location of tool_offset board, execute calibration moves, return
/// @param r_param (up to) millimeters of random jitter on X & Y axis during Z_probing each tool
/// @param probe_count number of Z probe repetitions per point to average
/// @return true if calibration was successful
bool run(uint8_t r_param, uint8_t probe_count);

} // namespace tool_offset_calibration

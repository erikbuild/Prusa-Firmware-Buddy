/// @file
#pragma once

#include <optional>
#include <cstdint>

/// Shows a dialog that prompts the user to select which docks to calibrate
/// @param dock_count number of docks to show (1-8)
/// @returns bitmask of selected docks, or std::nullopt if aborted
std::optional<uint8_t> select_docks_dialog(uint8_t dock_count);

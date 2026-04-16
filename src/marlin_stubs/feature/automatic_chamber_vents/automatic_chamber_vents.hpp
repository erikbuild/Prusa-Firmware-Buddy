/// @file
#pragma once

#include <option/has_chamber_vents.h>
#include <feature/chamber/chamber.hpp>

static_assert(HAS_CHAMBER_VENTS());

namespace automatic_chamber_vents {

using VentState = buddy::Chamber::VentState;

/// @brief Opens/closes the printer's vent grille.
/// @return true if the operation was successful, false otherwise.
bool execute_control(VentState target_state);

} // namespace automatic_chamber_vents

#pragma once

#include <str_utils.hpp>
#include <gcode_loader.hpp>

namespace nozzle_cleaner {

extern ConstexprString load_sequence;
extern ConstexprString unload_sequence;
extern ConstexprString runout_sequence;
extern ConstexprString g12_sequence;

extern ConstexprString load_filename;
extern ConstexprString unload_filename;
extern ConstexprString runout_filename;
extern ConstexprString g12_filename;

void load_load_gcode();
void load_runout_gcode();
void load_unload_gcode();
void load_g12_gcode();

bool is_loader_idle();
bool is_loader_buffering();

/**
 * @brief Executes the loaded nozzle cleaner gcode.
 * The load_xxx_gcode() function must be called before this function, and gcode loaded must be in ready state for this to work correctly.
 *
 * @return true if the gcode was executed successfully
 * @return false if still buffering, failed loading or not even loaded.
 */
bool execute();

} // namespace nozzle_cleaner

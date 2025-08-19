#include "nozzle_cleaner.hpp"
#include "Marlin/src/gcode/gcode.h"

namespace nozzle_cleaner {

ConstexprString load_sequence = "G1 X267.4 Y284.75 F3000\n"
                                "G1 X253.4 Y284.75 F3000\n"
                                "G1 X267.4 Y284.75 F3000\n"
                                "G1 X253.4 Y284.75 F3000\n"
                                "G1 X222.49 Y303.28 F5000\n"
                                "G1 X240.88 Y284.89 F2000\n"
                                "G1 X243.2 Y296.12\n"
                                "G1 X232.48 Y285.4\n"
                                "G1 X238.46 Y300.86\n"
                                "G1 X227.74 Y290.14\n"
                                "G1 X233.72 Y305.6\n"
                                "G1 X223 Y294.88\n"
                                "G1 X238.46 Y300.86\n"
                                "G1 X227.74 Y290.14\n"
                                "G1 X243.2 Y296.12\n"
                                "G1 X243.71 Y287.72\n"
                                "G1 X225.32 Y306.11\n"
                                "G1 X227.74 Y290.14\n"
                                "G1 X233.72 Y305.6\n"
                                "G1 X240.88 Y284.89\n"
                                "G27";

ConstexprString unload_sequence = "G1 X267.4 Y284.75 F3000\n"
                                  "G1 X253.4 Y284.75 F3000\n"
                                  "G1 X267.4 Y284.75 F3000\n"
                                  "G1 X253.4 Y284.75 F3000\n"
                                  "G27";

ConstexprString runout_sequence = "G1 X267.4 Y284.75 F3000\n"
                                  "G1 X253.4 Y284.75 F3000\n"
                                  "G1 X267.4 Y284.75 F3000\n"
                                  "G1 X253.4 Y284.75 F3000\n"
                                  "G1 X222.49 Y303.28 F5000\n"
                                  "G1 X240.88 Y284.89 F2000\n"
                                  "G1 X243.2 Y296.12\n"
                                  "G1 X232.48 Y285.4\n"
                                  "G1 X238.46 Y300.86\n"
                                  "G1 X227.74 Y290.14\n"
                                  "G1 X233.72 Y305.6\n"
                                  "G1 X223 Y294.88\n"
                                  "G1 X238.46 Y300.86\n"
                                  "G1 X227.74 Y290.14\n"
                                  "G1 X243.2 Y296.12\n"
                                  "G1 X243.71 Y287.72\n"
                                  "G1 X225.32 Y306.11\n"
                                  "G1 X227.74 Y290.14\n"
                                  "G1 X233.72 Y305.6\n"
                                  "G1 X240.88 Y284.89";

ConstexprString g12_sequence = runout_sequence;

ConstexprString load_filename = "nozzle_cleaner_load";
ConstexprString unload_filename = "nozzle_cleaner_unload";
ConstexprString runout_filename = "nozzle_cleaner_runout";
ConstexprString g12_filename = "nozzle_cleaner_g12";

static GCodeLoader &nozzle_cleaner_gcode_loader_instance() {
    static GCodeLoader nozzle_cleaner_gcode_loader;
    return nozzle_cleaner_gcode_loader;
}

void load_load_gcode() {
    nozzle_cleaner_gcode_loader_instance().load_gcode(load_filename, load_sequence);
}

void load_runout_gcode() {
    nozzle_cleaner_gcode_loader_instance().load_gcode(runout_filename, runout_sequence);
}

void load_unload_gcode() {
    nozzle_cleaner_gcode_loader_instance().load_gcode(unload_filename, unload_sequence);
}

void load_g12_gcode() {
    nozzle_cleaner_gcode_loader_instance().load_gcode(g12_filename, g12_sequence);
}

bool is_loader_idle() {
    return nozzle_cleaner_gcode_loader_instance().is_idle();
}

bool is_loader_buffering() {
    return nozzle_cleaner_gcode_loader_instance().is_buffering();
}

bool execute() {
    // If we are idle or buffering there is no point in trying to execute but we dont want to reset if we are buffering so we just return false
    if (is_loader_idle() || is_loader_buffering()) {
        return false;
    }

    auto loader_result = nozzle_cleaner_gcode_loader_instance().get_result();

    // this means the gcode was loaded successfully -> ready to execute it
    if (loader_result.has_value()) {
        GcodeSuite::process_subcommands_now(loader_result.value());
        nozzle_cleaner_gcode_loader_instance().reset();
        return true;
    } else { // Here we have an error so we finished unsuccessfully and need to reset the loader for the next use
        nozzle_cleaner_gcode_loader_instance().reset();
        return false;
    }
}

} // namespace nozzle_cleaner

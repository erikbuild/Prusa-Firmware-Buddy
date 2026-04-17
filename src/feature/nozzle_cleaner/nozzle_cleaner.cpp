#include "nozzle_cleaner.hpp"
#include "Marlin/src/gcode/gcode.h"
#include "bsod/bsod.h"
#include "raii/scope_guard.hpp"
#include <gcode_loader.hpp>
#include <Marlin/src/Marlin.h>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <option/has_indx.h>

namespace nozzle_cleaner {

#if HAS_INDX()
static const SequenceGCode clean_gcode = {
    .filename = "clean_sequence",
    .sequence = "G750 Y98.5 F21000\n"
                "G750 X-9 F21000\n"
                "G750 X0.65 Y118.5 F18000\n"
                "G750 X0.0 Y98.5 F18000\n"
                "G750 X-0.5 Y118.5 F18000\n"
                "G750 X-0.1 Y98.5 F18000\n"
                "G750 X-1.5 Y118.5 F18000\n"
                "G750 X-2 Y98.5 F18000\n"
                "G750 X-2 Y118.5 F18000\n"
                "G750 X0.65 Y96.5 F18000",
};

static const SequenceGCode quick_clean_gcode = {
    .filename = "quick_clean_sequence",
    .sequence = "G1 F21000\n"
                "G750 Y98.5 X0.65\n"
                "G750 Y118.5 X-0.35\n"
                "G750 Y98.5 X-1.35",
};

static const SequenceGCode deep_clean_gcode = {
    .filename = "deep_clean_sequence",
    .sequence = "G750 Y98.5 F21000\n"
                "G750 Y118.5 F21000\n"
                "G750 Y98.5 F21000\n"
                "G750 Y118.5 F21000\n"
                "G750 Y98.5 F21000\n"
                "G750 Y118.5 F21000\n"
                "G750 Y92.0 F21000\n"
                "G750 Y118.5 F21000\n"
                "G750 Y98.5 X0.15 F21000\n"
                "G750 Y118.5 X-0.35 F21000\n"
                "G750 Y98.5 X-0.85 F21000\n"
                "G750 Y118.5 X-1.35 F21000\n"
                "G750 Y98.5 X-1.85 F21000\n"
                "G750 Y118.5 X-2.35 F21000\n"
                "G750 Y98.5 X-2.85 F21000\n"
                "G750 Y118.5 X-3.35 F21000\n"
                "G750 Y98.5 X-3.85 F21000\n"
                "G750 Y118.5 X-4.35 F21000\n"
                "G750 Y98.5 X-4.85 F21000\n"
                "G750 Y118.5 X-5.35 F21000\n"
                "G750 Y98.5 X-5.85 F21000\n"
                "G750 Y118.5 X-6.35 F21000\n"
                "G750 Y98.5 X-6.85 F21000\n"
                "G750 Y118.5 X-7.35 F21000\n"
                "G750 Y98.5 X-7.85\n"
                "G750 X-9 F21000",
};

static const SequenceGCode purge_clean_gcode = {
    .filename = "purge_clean_sequence",
    .sequence = "G750 Y87 F21000\n" // Eject poop and move back to purge position
                "G750 Y91 F21000\n"
                "G750 Y84 F21000\n"
                "G750 Y91 F21000\n"
                "G750 Y77 F21000\n"
                "G750 Y91 F21000\n"
                "G750 Y86.5 F21000\n"
                "G1 E25 F250\n"
                "M400\n" // planner.synchronize()
                "G1 E8 F600\n"
                "M400\n"
                "G1 E-8 F6000\n"
                "M400\n"
                "G1 E1 F1440\n"
                "M400\n"
                "G1 E-1 F1440\n"
                "M400\n"
                "G750 Y98.5 F21000\n"
                "G750 Y91.5 F21000",
};

static const SequenceGCode eject_blob_gcode = {
    .filename = "eject_blob_sequence",
    .sequence = "M204 T5000\n"
                "G750 X-9 F21000\n" // Entry point (first in X) (to avoid hitting the nozzle cleaner with the nozzle)
                "G750 Y98.5 F21000\n" // Entry point
                "G750 X0.65 F21000\n"
                "G750 Y87 F21000\n"
                "G750 Y91 F21000\n"
                "G750 Y84 F21000\n"
                "G750 Y91 F21000\n"
                "G750 Y77 F21000\n"
                "G750 Y91 F21000\n"
                "G750 Y86.5 F21000\n",
};

static const SequenceGCode enter_cleaner_gcode = {
    .filename = "enter_cleaner_sequence",
    .sequence = "G750 Y98.5 F21000\n"
                "G750 X0.65 F10000",
};

static const SequenceGCode exit_cleaner_gcode = {
    .filename = "exit_cleaner_sequence",
    .sequence = "G750 Y98.5 F21000\n"
                "G750 X-9 F10000",
};

#else
static const SequenceGCode clean_gcode = {
    .filename = "nozzle_cleaner_clean",
    .sequence = "M106 S80\n" // fan on
                "G4 S2\n" // Wait for 2 seconds
                "G1 X267.4 Y284.75 F3000\n"
                "G1 X253.4 Y284.75 F3000\n"
                "G1 X267.4 Y284.75 F3000\n"
                "G1 X253.4 Y284.75 F3000\n"
                "G1 X253.4 Y305.0 F3000\n"
                "M106 S0\n" // fan off
                "G1 X254 Y285 F5000\n"
                "G1 X248 Y299 F5000\n"
                "G1 X235 Y285 F5000\n"
                "G1 X243 Y304 F5000\n"
                "G1 X230 Y291 F5000\n"
                "G1 X235 Y306 F5000\n"
                "G1 X224 Y296 F5000\n"
                "G1 X226 Y306 F3000\n"
                "G1 X248 Y288 F3000\n"
                "G1 X247 Y284 F3000\n"
                "G1 X229 Y306 F3000\n"
                "G1 X254 Y285 F5000\n"
                "G1 X248 Y299 F5000\n"
                "G1 X235 Y285 F5000\n"
                "G1 X243 Y304 F5000\n"
                "G1 X230 Y291 F5000\n"
                "G1 X235 Y306 F5000\n"
                "G1 X224 Y296 F5000\n"
                "G1 X226 Y306 F3000\n"
                "G1 X248 Y288 F3000\n"
                "G1 X247 Y284 F3000\n"
                "G1 X229 Y306 F3000",
};

static const SequenceGCode purge_clean_gcode = {
    .filename = "nozzle_cleaner_purge_clean",
    .sequence = "M106 S200\n" // fan on
                "G4 S4\n" // Wait for 4 seconds
                "G1 X267.4 Y284.75 F3000\n"
                "G1 X253.4 Y284.75 F3000\n"
                "G1 X267.4 Y284.75 F3000\n"
                "G1 X253.4 Y284.75 F3000\n"
                "G1 X253.4 Y305.0 F3000\n"
                "M106 S0\n", // fan off
};

#endif

static GCodeLoader &nozzle_cleaner_gcode_loader_instance() {
    static GCodeLoader nozzle_cleaner_gcode_loader;
    return nozzle_cleaner_gcode_loader;
}

std::optional<Sequence> parse_sequence(std::string_view name) {
    struct Entry {
        std::string_view name;
        Sequence seq;
    };
    static constexpr Entry entries[] = {
        { "clean", Sequence::clean },
#if HAS_INDX()
        { "quick_clean", Sequence::quick_clean },
        { "deep_clean", Sequence::deep_clean },
#endif
        { "purge_clean", Sequence::purge_clean },
#if HAS_INDX()
        { "eject_blob", Sequence::eject_blob },
        { "enter_cleaner", Sequence::enter_cleaner },
        { "exit_cleaner", Sequence::exit_cleaner },
#endif
    };
    for (const auto &e : entries) {
        if (e.name == name) {
            return e.seq;
        }
    }
    return std::nullopt;
}

bool is_valid_sequence(Sequence seq) {
    switch (seq) {
    case Sequence::clean:
    case Sequence::purge_clean:
#if HAS_INDX()
    case Sequence::quick_clean:
    case Sequence::deep_clean:
    case Sequence::eject_blob:
    case Sequence::enter_cleaner:
    case Sequence::exit_cleaner:
#endif
        return true;
    }
    return false;
}

const SequenceGCode &get_sequence(Sequence seq) {
    switch (seq) {
    case Sequence::clean:
        return clean_gcode;
    case Sequence::purge_clean:
        return purge_clean_gcode;
#if HAS_INDX()
    case Sequence::quick_clean:
        return quick_clean_gcode;
    case Sequence::deep_clean:
        return deep_clean_gcode;
    case Sequence::eject_blob:
        return eject_blob_gcode;
    case Sequence::enter_cleaner:
        return enter_cleaner_gcode;
    case Sequence::exit_cleaner:
        return exit_cleaner_gcode;
#endif
    }
    bsod_unreachable();
}

void load_sequence(Sequence seq) {
    const auto &gcode = get_sequence(seq);
    nozzle_cleaner_gcode_loader_instance().load_gcode(gcode.filename, gcode.sequence);
}

bool load_and_execute(Sequence seq) {
    while (true) {
        if (planner.draining()) {
            return false;
        }
        if (is_loader_idle()) {
            load_sequence(seq);
            break;
        }
        idle(true);
    }

    while (!execute()) {
        if (planner.draining()) {
            return false;
        }
        idle(true);
    }

    return true;
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

    // skip the execution if XY is not homed; we could save ourselves the
    // whole gcode loading too, but that'd add extra conditions and edge cases
    if (!(axes_home_level.is_homed(X_AXIS, AxisHomeLevel::imprecise) && axes_home_level.is_homed(Y_AXIS, AxisHomeLevel::imprecise))) {
        reset();
        return true;
    }

    auto loader_result = nozzle_cleaner_gcode_loader_instance().get_result();
    ScopeGuard resetLoader = [&] { // Ensure the loader is always reset (the exception is if we are buffering or not idle, which is handled above)
        reset();
    };

    // this means the gcode was loaded successfully -> ready to execute it
    if (loader_result.has_value()) {
        GcodeSuite::process_subcommands_now(loader_result.value());
        return true;
    } else { // Here we have an error so we finished unsuccessfully and need to reset the loader for the next use
        return false;
    }
}

void reset() {
    nozzle_cleaner_gcode_loader_instance().reset();
}

} // namespace nozzle_cleaner

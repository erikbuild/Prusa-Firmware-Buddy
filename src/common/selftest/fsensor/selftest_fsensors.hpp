/// \file
#pragma once

#include <tool_index.hpp>

struct SelftestFSensorsParams {
    PhysicalToolIndex tool;
};

enum class SelftestFSensorsResult {
    success,
    failed,

    /// The selftest wasn't able to finish, either is has been aborted by the user or by failing some preconditions
    aborted
};

SelftestFSensorsResult run_selftest_fsensors(const SelftestFSensorsParams &params);

/// @file
#pragma once

#include "base_hotend.hpp"

/// Represents a hotend that is controlled on the current processor (not on a dwarf)
class LocalHotend final : public BaseHotend {

public:
    explicit LocalHotend(PhysicalToolIndex tool)
        : BaseHotend(tool) {}
};

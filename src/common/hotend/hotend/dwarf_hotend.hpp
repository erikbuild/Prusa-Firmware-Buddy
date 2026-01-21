/// @file
#pragma once

#include "base_hotend.hpp"

/// Represents a hotend that is managed by a dwarf on an XL
class DwarfHotend final : public BaseHotend {

public:
    explicit DwarfHotend(PhysicalToolIndex tool)
        : BaseHotend(tool) {}
};

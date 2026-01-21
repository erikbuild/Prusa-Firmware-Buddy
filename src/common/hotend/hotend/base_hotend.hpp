/// @file
#pragma once

#include <hotend/hotend.hpp>

/// Represents a base for all non-dummy hotends
class BaseHotend : public Hotend {

protected:
    explicit BaseHotend(PhysicalToolIndex tool);

protected:
    const PhysicalToolIndex tool_;
};

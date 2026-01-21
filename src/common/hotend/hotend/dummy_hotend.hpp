/// @file
#pragma once

#include <hotend/hotend.hpp>

/// Represents a hotend that does nothing at all
/// Used for NoTool hotends
class DummyHotend final : public Hotend {

public:
    using Hotend::Hotend;
};

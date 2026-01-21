/// @file
#pragma once

#include <tool_index.hpp>
#include <utils/uncopyable.hpp>
#include <utils/variant_utils.hpp>

/// Class representing a hotend
/// This is an abstract class, hotend implementations differ
class Hotend : public Uncopyable {
    friend class Temperature;

public:
    /// in °C
    using Temperature = float;
    static constexpr Temperature temperature_uninitialized = -1;

    /// in °C
    /// <= 0 = no target temperature/invalid value
    using TargetTemperature = int16_t;

public:
    /// @returns Hotend of the tool
    /// There should be a 1:1 mapping
    /// Implemented differently for each printer in in hotends_XX.cpp
    /// !!! To be accessed only from the marlin task
    static Hotend &for_tool(PhysicalToolIndex tool);

    static Hotend &for_tool(std::variant<PhysicalToolIndex, NoTool> tool);

    [[deprecated("Use the strong typed variant")]]
    static Hotend &for_tool(uint8_t tool);

protected:
    explicit Hotend() = default;
};

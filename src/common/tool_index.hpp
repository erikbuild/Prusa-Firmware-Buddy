/// @file
#pragma once

#include <cstdint>
#include <array>

#include <inc/MarlinConfig.h>
#include <common/array_extensions.hpp>
#include <bsod.h>

/// Strong type for reprezenting no tool using `std::variant<SomeToolIndex, NoTool>`
struct NoTool {};

/// Strong base type for indexing tools, providing common functionality between PhysicalToolIndex, VirtualToolIndex and GcodeToolIndex
template <const int count_, template <typename> typename Extension>
struct ToolIndex : public Extension<ToolIndex<count_, Extension>> {
public:
    /// @param index must be less than ToolIndex::count
    /// Creates ToolIndex from raw uint8_t
    /// @deprecated Replace raw index with ToolIndex for better safety
    static inline constexpr ToolIndex from_raw(uint8_t index) {
        return ToolIndex(index);
    }

    /// @returns raw index as uint8_t
    /// @deprecated Replace raw index with ToolIndex for better safety
    inline constexpr uint8_t to_raw() const { return this->value; }

    /// Maximum number of tools the firmware supports
    static constexpr uint8_t count = count_;

    /// List of all tools the printer offers, for `for()` loops
    static consteval std::array<ToolIndex, count> all() {
        return stdext::make_iota_array<count, from_raw>();
    }

    constexpr bool operator==(const ToolIndex &) const = default;

private:
    inline explicit constexpr ToolIndex(uint8_t index)
        : value(index) {
        if (index >= count) {
            bsod("ToolIndex out of range");
        }
    }

private:
    uint8_t value;
};

template <typename Derived>
struct PhysicalToolIndexExtension {};

/// Strong type for indexing physical tools.
/// Physical tool is a single "toolhead" a printer has.
/// This more or less corresponds with the old marlin HOTENDS macro.
/// Each toolhead has properties like hotend temperature, nozzle offset, ...
/// - XL has (up to) 5 physical tools
/// - INDX has a lot if physical tools (even though only one nozzle can be heated at a time)
/// - MINI, MKx, C1 (non-index) have a single physical tool
using PhysicalToolIndex = ToolIndex<HOTENDS, PhysicalToolIndexExtension>;

template <typename Derived>
struct VirtualToolIndexExtension {
    PhysicalToolIndex to_physical() const;
};

/// Strong type for indexing "virtual" tools.
/// Virtual tools extend the physical tools with the possibility of filament multiplexing,
/// where multiple virtual tools can be mapped to a single physical tool and swapped through multiplexing (MMU).
/// This more or less corresponds with the old marlin EXTRUDERS macro.
/// Used virtual tools are surjectively mapped to physical tools.
using VirtualToolIndex = ToolIndex<EXTRUDERS, VirtualToolIndexExtension>;

template <typename Derived>
struct GcodeToolIndexExtension {};

/// Strong type for indexing virtual tools before toolmapping.
/// This type corresponds to the tool indexes as they are used in the gcode.
/// These indexes are then mapped to VirtualToolIndex, based on the current tool mapping configuration.
/// There is a bijection relation between the active virtual tools and gcode tools,
/// (spool join kind of extends this to surjection, but at any single moment, a GCodeTool is always mapped to a single VirtualTool)
using GcodeToolIndex = ToolIndex<VirtualToolIndex::count, GcodeToolIndexExtension>;

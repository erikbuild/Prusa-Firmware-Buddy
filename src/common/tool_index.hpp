/// @file
#pragma once

#include <cstdint>
#include <array>
#include <variant>

#include <inc/MarlinConfig.h>
#include <common/array_extensions.hpp>
#include <bsod.h>
#include <module/prusa/toolchanger.h>
#include <module/prusa/tool_mapper.hpp>

/// Strong type for reprezenting no tool using `std::variant<SomeToolIndex, NoTool>`
struct NoTool {};

/// Strong base type for indexing tools, providing common functionality between PhysicalToolIndex, VirtualToolIndex and GcodeToolIndex
template <const int count_, template <typename> typename Extension>
struct ToolIndex : public Extension<ToolIndex<count_, Extension>> {
public:
    /// Creates ToolIndex from raw uint8_t
    /// Use `from_raw_notool` instead, if you are not sure that raw index represent only valid tool
    /// @param index must be less than ToolIndex::count
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
struct PhysicalToolIndexExtension {
    /// Checks for legacy values representing no tool
    /// Use `from_raw` instead, if you are sure that raw index represent only valid tool
    /// @param index
    /// @deprecated This function should be removed after removing all special (notool) values in raw indices
    static inline constexpr std::variant<Derived, NoTool> from_raw_notool(uint8_t index) {
#if PRINTER_IS_PRUSA_XL()
        // count on XL is set to EXTRUDERS - 1 / HOTENDS - 1,
        // because of legacy definition of EXTRUDERS / HOTENDS to 6 instead of 5.
        // Values 0 to 4 are actual tools, 5 represents no tool (using MARLIN_NO_TOOL_PICKED).
        // If we encounter MARLIN_NO_TOOL_PICKED we need to return NoTool and check for it on call site.
        static_assert(Derived::count == PrusaToolChanger::MARLIN_NO_TOOL_PICKED);
        if (index == PrusaToolChanger::MARLIN_NO_TOOL_PICKED) {
            return NoTool {};
        }
#endif
        // Other non-tool values treat as invalid values -> raise bsod
        return Derived(index);
    }
};

/// Strong type for indexing physical tools.
/// Physical tool is a single "toolhead" a printer has.
/// This more or less corresponds with the old marlin HOTENDS macro.
/// Each toolhead has properties like hotend temperature, nozzle offset, ...
/// - XL has (up to) 5 physical tools
/// - INDX has a lot if physical tools (even though only one nozzle can be heated at a time)
/// - MINI, MKx, C1 (non-index) have a single physical tool
#if PRINTER_IS_PRUSA_XL()
// HOTENDS is set to 6 instead of 5.
// Values 0 to 4 represents tools and 5 represents no tool, which we need to disallow in strong types.
// For representing no tool we will use NoTool type instead.
static_assert(HOTENDS == 6);
using PhysicalToolIndex = ToolIndex<HOTENDS - 1, PhysicalToolIndexExtension>;
#else
using PhysicalToolIndex = ToolIndex<HOTENDS, PhysicalToolIndexExtension>;
#endif

template <typename Derived>
struct VirtualToolIndexExtension {
    PhysicalToolIndex to_physical() const;

    /// Checks for legacy values representing no tool
    /// Use `from_raw` instead, if you are sure that raw index represent only valid tool
    /// @param index
    /// @deprecated This function should be removed after removing all special (notool) values in raw indices
    static inline constexpr std::variant<Derived, NoTool> from_raw_notool(uint8_t index) {
#if HAS_TOOL_MAPPING()
        // There is 255 (NO_TOOL_MAPPED) used as no tool at some places.
        if (index == ToolMapper::NO_TOOL_MAPPED) {
            return NoTool {};
        }
#endif
        // Other non-tool values treat as invalid values -> raise bsod
        return Derived(index);
    }
};

/// Strong type for indexing "virtual" tools.
/// Virtual tools extend the physical tools with the possibility of filament multiplexing,
/// where multiple virtual tools can be mapped to a single physical tool and swapped through multiplexing (MMU).
/// This more or less corresponds with the old marlin EXTRUDERS macro.
/// Used virtual tools are surjectively mapped to physical tools.
#if PRINTER_IS_PRUSA_XL()
// EXTRUDERS is set to 6 instead of 5.
// Values 0 to 4 represents tools and 5 represents no tool, which we need to disallow in strong types.
// For representing no tool we will use NoTool type instead.
static_assert(EXTRUDERS == 6);
using VirtualToolIndex = ToolIndex<EXTRUDERS - 1, VirtualToolIndexExtension>;
#else
using VirtualToolIndex = ToolIndex<EXTRUDERS, VirtualToolIndexExtension>;
#endif

template <typename Derived>
struct GcodeToolIndexExtension {};

/// Strong type for indexing virtual tools before toolmapping.
/// This type corresponds to the tool indexes as they are used in the gcode.
/// These indexes are then mapped to VirtualToolIndex, based on the current tool mapping configuration.
/// There is a bijection relation between the active virtual tools and gcode tools,
/// (spool join kind of extends this to surjection, but at any single moment, a GCodeTool is always mapped to a single VirtualTool)
using GcodeToolIndex = ToolIndex<VirtualToolIndex::count, GcodeToolIndexExtension>;

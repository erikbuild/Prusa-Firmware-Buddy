/// @file
#pragma once

#include <cstdint>
#include <variant>

#include <inc/MarlinConfig.h>
#include <common/array_extensions.hpp>
#include <utils/storage/strong_index_array.hpp>
#include <bsod.h>
#include <utils/overloaded_visitor.hpp>
#include <option/board_is_master_board.h>
#include <printers.h>
#include <string_view_utf8.hpp>

#include "tool_index_iterator.hpp"

/// Strong type for representing no tool using `std::variant<SomeToolIndex, NoTool>`
struct NoTool {
    inline constexpr bool operator==(const NoTool &) const {
        return true;
    }
};

/// Strong type for representing all tools using `std::variant<SomeToolIndex, AllTools>`
struct AllTools {
    inline constexpr bool operator==(const AllTools &) const {
        return true;
    }
};

/// Strong base type for indexing tools, providing common functionality between PhysicalToolIndex, VirtualToolIndex and GcodeToolIndex
template <const int count_, template <typename> typename Extension>
struct ToolIndex : public Extension<ToolIndex<count_, Extension>> {

public:
    using Iterator = ToolIndexIterator<ToolIndex>;

    constexpr inline ToolIndex(const ToolIndex &) = default;

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

    /// @returns Index of the tool, starting from 1, for display purposes
    inline constexpr uint8_t display_index() const {
        return to_raw() + 1;
    }

    using DisplayNameParams = StringViewUtf8Parameters<4>;

    /// @returns display name of the tool - something like "Slot 1" or "Tool 1"
    string_view_utf8 display_name(DisplayNameParams &params);

    /// Allow simpler conversion to integer when using StrongIndexArray
    /// @note pass-by-reference is needed to avoid circular dependencies
    static inline constexpr std::size_t to_raw_static(const ToolIndex &tool_index) {
        return tool_index.to_raw();
    }

    /// Maximum number of tools the firmware supports
    static constexpr uint8_t count = count_;

    /// List of all tools the printer offers, for `for()` loops
    static consteval Iterator all() {
        return Iterator::make_all();
    }

    inline constexpr bool operator==(const ToolIndex &other) const {
        return value == other.value;
    }

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
    /// @returns whether the specified tool is enabled
    /// Disabled tools cannot be selected and used for printing
    bool is_enabled() const;

    /// Checks for legacy values representing no tool
    /// Use `from_raw` instead, if you are sure that raw index represent only valid tool
    /// @param index
    /// @deprecated This function should be removed after removing all special (notool) values in raw indices
    static std::variant<Derived, NoTool> from_raw_notool(uint8_t index);

    /// @returns currently picked physical tool
    /// @note There can be a currrently_active() physical tool without having any currently_active() virtual one.
    /// @note That would for example mean that a tool is picked but a MMU for that tool does not have any slot loaded in.
    static std::variant<Derived, NoTool> currently_selected();
};

/// Strong type for indexing physical tools.
/// Physical tool is a single "toolhead" a printer has.
/// This more or less corresponds with the old marlin HOTENDS macro.
/// Each toolhead has properties like hotend temperature, nozzle offset, ...
/// - XL has (up to) 5 physical tools
/// - INDX has a lot if physical tools (even though only one nozzle can be heated at a time)
/// - MINI, MKx, C1 (non-index) have a single physical tool
#if PRINTER_IS_PRUSA_XL() && BOARD_IS_MASTER_BOARD()
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
    /// @returns whether the specified tool is enabled
    /// Disabled tools cannot be selected and used for printing
    bool is_enabled() const;

    PhysicalToolIndex to_physical() const;

    /// Checks for legacy values representing no tool
    /// Use `from_raw` instead, if you are sure that raw index represent only valid tool
    /// @param index
    /// @deprecated This function should be removed after removing all special (notool) values in raw indices
    static std::variant<Derived, NoTool> from_raw_notool(uint8_t index);

    /// @returns currently active virtual tool
    /// Active = the corresponding physical tool is active and has the resulting virtual tool selected
    static std::variant<Derived, NoTool> currently_selected();
};

/// Strong type for indexing "virtual" tools.
/// Virtual tools extend the physical tools with the possibility of filament multiplexing,
/// where multiple virtual tools can be mapped to a single physical tool and swapped through multiplexing (MMU).
/// This more or less corresponds with the old marlin EXTRUDERS macro.
/// Used virtual tools are surjectively mapped to physical tools.
#if PRINTER_IS_PRUSA_XL() && BOARD_IS_MASTER_BOARD()
// EXTRUDERS is set to 6 instead of 5.
// Values 0 to 4 represents tools and 5 represents no tool, which we need to disallow in strong types.
// For representing no tool we will use NoTool type instead.
static_assert(EXTRUDERS == 6);
using VirtualToolIndex = ToolIndex<EXTRUDERS - 1, VirtualToolIndexExtension>;
#else
using VirtualToolIndex = ToolIndex<EXTRUDERS, VirtualToolIndexExtension>;
#endif

template <typename Derived>
struct GcodeToolIndexExtension {
    /// @returns whether the specified tool is enabled
    /// Disabled tools cannot be selected and used for printing
    bool is_enabled() const;

    /// @returns VirtualToolIndex corresponding to the GCodeToolIndex, if there is any
    /// Will return NoTool if no tool is mapped
    std::variant<VirtualToolIndex, NoTool> to_virtual() const;

    /// @returns PhysicalToolIndex corresponding to the GCodeToolIndex, if there is any
    /// Will return NoTool if no tool is mapped
    std::variant<PhysicalToolIndex, NoTool> to_physical() const;
};

/// Strong type for indexing virtual tools before toolmapping.
/// This type corresponds to the tool indexes as they are used in the gcode.
/// These indexes are then mapped to VirtualToolIndex, based on the current tool mapping configuration.
/// There is a bijection relation between the active virtual tools and gcode tools,
/// (spool join kind of extends this to surjection, but at any single moment, a GCodeTool is always mapped to a single VirtualTool)
using GcodeToolIndex = ToolIndex<VirtualToolIndex::count, GcodeToolIndexExtension>;

/// @returns a variant where all the types are mapped to PhysicalToolIndex alternatives
/// NoTool and AllTools are unchanged
/// VirtualToolIndex and GCodeToolIndex are mapped using to_physical()
/// @example to_physical_tool_index<NoTool>(std::variant<VirtualToolIndex, NoTool>(VirtualToolIndex::from_raw(0))) -> std::variant<PhysicalToolIndex, NoTool>
template <typename... Variants, typename... T>
auto to_physical_tool_index(const std::variant<T...> &variant) {
    // Note: cannot use match() here because not all overloads would necessarily be in the variant
    static constexpr auto f = []<typename V>(V val) -> std::variant<PhysicalToolIndex, Variants...> {
        if constexpr (std::is_same_v<V, GcodeToolIndex> || std::is_same_v<V, VirtualToolIndex>) {
            return val.to_physical();

        } else if constexpr (std::is_same_v<V, NoTool> || std::is_same_v<V, AllTools>) {
            return val;

        } else {
            static_assert(false);
        }
    };
    return std::visit(f, variant);
}

/// @returns range of tools represented by the variant
/// NoTool returns an empty range
/// AllTools returns a range of all tools
/// Tool returns a single item range
template <typename Index, typename... Variants>
auto tool_index_iterator(const std::variant<Index, Variants...> &variant) {
    using Iterator = Index::Iterator;
    return match(
        variant,
        [](Index i) { return Iterator::make_single(i); },
        [](NoTool) { return Iterator::make_empty(); },
        [](AllTools) { return Iterator::make_all(); });
}

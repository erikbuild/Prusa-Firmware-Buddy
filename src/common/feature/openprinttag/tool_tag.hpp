/// @file
#pragma once

#include <cstdint>
#include <optional>

#include <tool_index.hpp>

#include <feature/openprinttag/detail/defines.hpp>

namespace buddy::openprinttag {

inline bool has_tool_openprinttag_reader([[maybe_unused]] VirtualToolIndex tool) {
    // TODO
    return true;
}

struct ToolTagField;

/// Class representing a specific tag associated with a specific tool
/// The value gets "invalidated" if a tag is removed from the tool
class ToolTag {

public:
    using UIDHash = uint16_t;

    /// Used in config store
    static constexpr UIDHash no_tag_hash = 0;

public:
    /// @returns tag that is currently detected at the specified tool spool slot
    /// This can be a different tag than @p for_tool_assigned in case the user removes the assignes pool
    /// !!! NOT to be used for anything related to printing, use for_tool_assigned for that
    static std::optional<ToolTag> for_tool_ephemeral(VirtualToolIndex tool) {
        // Just a stub for testing
        // TODO proper implementation
        if (tool.to_raw() < 2) {
            return ToolTag { tool, 1 };
        } else {
            return std::nullopt;
        }
    }

    /// @returns tag that is assigned to the specified tool
    /// This is a long-term assignment that happens during filament load, and is unassigned on unload
    static std::optional<ToolTag> for_tool_assigned(VirtualToolIndex tool);

public:
    inline VirtualToolIndex tool() const {
        return tool_;
    }

    inline UIDHash uid_hash() const {
        return uid_hash_;
    }

    /// @returns a struct representing a specific field on the tag
    template <typename F>
    inline ToolTagField field(F field) const;

    constexpr inline bool operator==(const ToolTag &) const = default;
    constexpr inline bool operator!=(const ToolTag &) const = default;

private:
    explicit inline ToolTag(VirtualToolIndex tool, UIDHash uid_hash)
        : tool_(tool)
        , uid_hash_(uid_hash) {}

private:
    VirtualToolIndex tool_;

    /// Hash of the tag UID (to reduce size)
    /// This ensures that we don't accidentally read/write something from a different tag (if tag for a tool changes suddenly)
    UIDHash uid_hash_;
};

struct ToolTagField {
    ToolTag tag;
    Section section;
    Field field;

    constexpr inline bool operator==(const ToolTagField &) const = default;
    constexpr inline bool operator!=(const ToolTagField &) const = default;
};

template <typename F>
inline ToolTagField ToolTag::field(F field) const {
    return ToolTagField {
        .tag = *this,
        .section = ::openprinttag::field_section(field),
        .field = static_cast<Field>(field),
    };
}

}; // namespace buddy::openprinttag

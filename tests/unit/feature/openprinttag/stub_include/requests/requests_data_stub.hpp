#pragma once

#include <utility>
#include <any>
#include <unordered_map>

#include <feature/openprinttag/tool_tag.hpp>

namespace buddy::openprinttag {

struct ToolTagFieldHash {
    using hash_type = std::hash<ToolTagField>;
    using is_transparent = void;

    std::size_t operator()(ToolTagField f) const {
        return std::hash<Section>()(f.section) ^ std::hash<Field>()(f.field) ^ std::hash<uint8_t>()(f.tag.tool().to_raw()) ^ std::hash<ToolTag::UIDHash>()(f.tag.uid_hash());
    }
};

using StubData = std::unordered_map<ToolTagField, std::any, ToolTagFieldHash>;
inline thread_local StubData stub_data;
inline thread_local size_t write_count = 0;

} // namespace buddy::openprinttag

#include "tool_tag.hpp"

#include <config_store/store_instance.hpp>

namespace buddy::openprinttag {

std::optional<ToolTag> ToolTag::for_tool_assigned(VirtualToolIndex tool) {
    // Validate the config store item match
    using HashItem = decltype(config_store().opt_tool_assigned_tag);
    static_assert(std::is_same_v<HashItem::value_type, UIDHash>);
    static_assert(HashItem::default_val == no_tag_hash);

    const auto uid_hash = config_store().opt_tool_assigned_tag.get(tool.to_raw());
    if (uid_hash == no_tag_hash) {
        return std::nullopt;
    }

    return ToolTag { tool, uid_hash };
}

} // namespace buddy::openprinttag

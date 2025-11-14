/// @file
#include "tool_tag.hpp"

#include <bsod/bsod.h>

namespace buddy::openprinttag {

ToolTag::ToolTag(VirtualToolIndex tool, UIDHash uid_hash)
    : uid_hash_(uid_hash)
    , tool_(tool) {
    if (uid_hash_ == no_tag_hash) {
        bsod_unreachable();
    }
}

} // namespace buddy::openprinttag

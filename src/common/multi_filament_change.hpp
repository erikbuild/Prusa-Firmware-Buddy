#pragma once

#include <optional>
#include <array>

#include <filament.hpp>
#include <tool_index.hpp>
#include <color.hpp>

namespace multi_filament_change {

enum class Action : uint8_t {
    /// Keep as is, do not change the filament
    keep,

    /// Change filament to \p new_filament
    change,

    /// Unload the filament
    unload,
};

struct ConfigItem {
    Action action = Action::keep;
    FilamentType new_filament = FilamentType::none;
    std::optional<Color> color;
};

using Config = StrongIndexArray<ConfigItem, VirtualToolIndex::count, VirtualToolIndex, VirtualToolIndex::to_raw_static, strong_index_array::AllowWeakIndexing::yes>;

} // namespace multi_filament_change

/// Configuration used in DialogChangeAllFilaments
using MultiFilamentChangeConfig = multi_filament_change::Config;

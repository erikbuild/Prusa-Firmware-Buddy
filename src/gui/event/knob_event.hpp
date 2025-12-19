#pragma once

#include "gui_event.hpp"
#include <guitypes.hpp>

namespace gui_event {

/// Received when an a knob is rotated
/// This is passed to IWindowMenuItem only if it is_edited
struct KnobEvent {
    /// clockwise -> positive
    int diff;

    inline bool operator==(const KnobEvent &) const = default;
};
static_assert(GuiEventType<KnobEvent>);

}; // namespace gui_event

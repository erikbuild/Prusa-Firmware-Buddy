#pragma once

#include <gcode_compatibility.hpp>
#include <inplace_vector.hpp>
#include <window_frame.hpp>
#include <fsm/print_preview_phases.hpp>
#include <window_menu_virtual.hpp>
#include <dynamic_index_mapping.hpp>
#include <window_menu_adv.hpp>
#include <radio_button_fsm.hpp>
#include <window_text.hpp>

namespace screen_print_preview {

class WindowMenuGCodeIncompatible final : public WindowMenuVirtual {
public:
    WindowMenuGCodeIncompatible(window_t *parent, Rect16 rect, PhasesPrintPreview phase);

public:
    int item_count() const final;

protected:
    void setup_item(ItemVariant &variant, int index) final;

private:
    const PhasesPrintPreview phase_;

    using CheckMetadata = buddy::gcode_compatibility::CheckMetadata;

    // Note: this frame only shows tool-specific incompabilities for single-tool printers
    // So we don't need to store what tool the fails relate to
    // Ambiguous tool-specific fails should be handled by the tool mapping screen
    stdext::inplace_vector<const CheckMetadata *, 16> failed_checks_;
};

class FrameGCodeIncompatible {

public:
    FrameGCodeIncompatible(window_frame_t *parent, PhasesPrintPreview phase);

private:
    window_text_t title_;
    BasicWindow title_line_;
    WindowExtendedMenu<WindowMenuGCodeIncompatible> menu_;
    RadioButtonFSM radio_;
};

} // namespace screen_print_preview

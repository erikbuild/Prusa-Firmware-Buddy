#include "frame_gcode_incompatible.hpp"

#include <window_menu_callback_item.hpp>
#include <gcode_compatibility.hpp>
#include <tools_mapping.hpp>
#include <client_response_texts.hpp>
#include <marlin_client.hpp>
#include <img_resources.hpp>
#include <window_msgbox.hpp>
#include <gui/auto_layout.hpp>

using namespace buddy;

namespace screen_print_preview {

static constexpr IWindowMenuItem::ColorScheme fatal_check_color_scheme {
    .text {
        .focused = COLOR_RED,
        .unfocused = COLOR_RED,
    },
};

static constexpr IWindowMenuItem::ColorScheme failed_check_color_scheme {
    .text {
        .focused = COLOR_BRAND,
        .unfocused = COLOR_BRAND,
    },
};

WindowMenuGCodeIncompatible::WindowMenuGCodeIncompatible(window_t *parent, Rect16 rect, PhasesPrintPreview phase)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::no)
    , phase_(phase) {

    gcode_compatibility::CompatibilityReport report;
    if (tools_mapping::is_tool_mapping_possible()) {
        // Only report non-tool related problems
        // Tool checks will be handled on the tool mapping screen
        report.generate_without_toolmapping();

    } else {
        // There will be no separate tooomapping screen,
        // so show all problems, with the naive 1:1 toolmapping
        report.generate_full({});
    }

    report.visit_failed_checks([this](const auto &fail) {
        failed_checks_.push_back(fail.meta);
        return failed_checks_.size() != failed_checks_.max_size();
    });

    setup_items();
}

int screen_print_preview::WindowMenuGCodeIncompatible::item_count() const {
    return failed_checks_.size();
}

void screen_print_preview::WindowMenuGCodeIncompatible::setup_item(ItemVariant &variant, int index) {
    const auto &meta = *failed_checks_[index];
    const HWCheckSeverity severity = meta.evaluate_severity();

    const auto cb = [&meta] {
        if (meta.description) {
            MsgBoxInfo(_(meta.description), Responses_Ok);
        }
    };

    auto &item = variant.emplace<WindowMenuCallbackItem>(_(meta.title), cb);
    item.set_color_scheme(severity == HWCheckSeverity::Abort ? &fatal_check_color_scheme : &failed_check_color_scheme);

#if HAS_MINI_DISPLAY()
    item.setLabelFont(Font::small);
#else
    item.SetIconId(hw_check_severity_icons[severity]);
#endif

    if (meta.description) {
        item.set_show_expand_icon();
    }
}

FrameGCodeIncompatible::FrameGCodeIncompatible(window_frame_t *parent, PhasesPrintPreview phase)
    : title_(parent, Rect16 {}, is_multiline::yes)
    , title_line_(parent, {})
    , menu_(parent, Rect16 {}, phase)
    , radio_(parent, Rect16 {}, phase) {

    title_.SetAlignment(Align_t::LeftBottom());
    title_.SetText(_("G-Code incompatibilities detected"));
#if HAS_MINI_DISPLAY()
    title_.set_font(Font::small);
#endif

    title_line_.SetBackColor(COLOR_DARK_GRAY);

    static constexpr std::array layout {
        // Title
        StackLayoutItem {
            .height = 32,
            .margin_side = 16,
            .margin_top = 4,
            .margin_bottom = 4,
        },

        // Title line
        StackLayoutItem {
            .height = 2,
            .margin_side = 16,
            .margin_bottom = 4,
        },

        // Incompatibilities list (menu)
        StackLayoutItem {
            .height = StackLayoutItem::stretch,
#if !HAS_MINI_DISPLAY()
            .margin_side = 16,
#endif
        },

        // Radio
        standard_stack_layout::for_radio,
    };
    auto windows = std::to_array<window_t *>({
        &title_,
        &title_line_,
        &menu_,
        &radio_,
    });

    layout_vertical_stack(menu_.GetParent()->GetRect(), windows, layout);

    // Do NOT capture anything - the window frame itself should be able to handle passing events through
    // and make KNOB events focus previous/next element
    // parent->CaptureNormalWindow(nullptr);
    // But we gotta set focus to the radio
    radio_.SetFocus();
}

} // namespace screen_print_preview

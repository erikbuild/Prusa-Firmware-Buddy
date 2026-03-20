#include "frame_prompt.hpp"

#include <gui/auto_layout.hpp>
#include <find_error.hpp>
#include <bsod/bsod.h>
#include <guiconfig/GuiDefaults.hpp>
namespace {
static constexpr std::array layout_no_footer {
    StackLayoutItem { .height = 32, .margin_side = 16, .margin_bottom = 4 },
    StackLayoutItem { .height = 2, .margin_side = 16 },
    StackLayoutItem { .height = StackLayoutItem::stretch, .margin_side = 16, .margin_top = 16 },
    standard_stack_layout::for_radio,
};
static constexpr std::array layout_only_footer {
    standard_stack_layout::for_footer,
};
static constexpr std::array layout_with_footer = stdext::array_concat(layout_no_footer, layout_only_footer);
static_assert(layout_no_footer.size() + 1 == layout_with_footer.size(), "Layout without footer should be exactly one item (the footer) smaller than layout with footer");
} // namespace

FramePrompt::FramePrompt(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &txt_title, const string_view_utf8 &txt_info)
    : FramePrompt(parent, fsm_phase, txt_title, txt_info, Align_t::LeftTop(), Align_t::LeftBottom()) {}

FramePrompt::FramePrompt(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &txt_title, const string_view_utf8 &txt_info, const Align_t info_alignment, const Align_t title_alignment)
    : title(parent, {}, is_multiline::yes, is_closed_on_click_t::no, txt_title)
    , title_line(parent, {})
    , info(parent, {}, is_multiline::yes, is_closed_on_click_t::no, txt_info)
    , radio(parent, {}, fsm_phase) //
{
    title.SetAlignment(title_alignment);
    title.set_font(GuiDefaults::FontBig);
    title.SetTextColor(COLOR_BRAND);

    info.SetAlignment(info_alignment);
#if HAS_MINI_DISPLAY()
    info.set_font(Font::small);
#endif
    title_line.SetBackColor(COLOR_DARK_GRAY);

    parent->CaptureNormalWindow(radio);

    std::array<window_t *, layout_no_footer.size()> windows_no_footer { &title, &title_line, &info, &radio };
    layout_vertical_stack(parent->GetRect(), windows_no_footer, layout_no_footer);

    // Hide after layout is calculated - always take 1px line into account, text may be set later
    title_line.set_visible(!txt_title.isNULLSTR());
}

FramePrompt::FramePrompt(window_frame_t *parent, FSMAndPhase fsm_phase, std::optional<ErrCode> (*error_code_mapper)(FSMAndPhase fsm_phase), const Align_t info_alignment, const Align_t title_alignment)
    : FramePrompt(parent, fsm_phase, string_view_utf8::MakeNULLSTR(), string_view_utf8::MakeNULLSTR(), info_alignment, title_alignment) {

    // Extracting information: Phase -> corresponding error code -> message
    const auto err_code = error_code_mapper(fsm_phase);
    if (!err_code.has_value()) {
        bsod_unreachable(); // Some phases do not have corresponding error codes - they should not be called with this constructor
    }

    const auto err = find_error(err_code.value());

    info.SetText(_(err.err_text));
    title.SetText(_(err.err_title));

    // Show the line only if error has a title
    if (strlen(err.err_title) != 0) {
        title_line.Show();
    }
}

void FramePrompt::add_footer(FooterLine &footer) {
    std::array<window_t *, layout_with_footer.size()> windows_with_footer { &title, &title_line, &info, &radio, &footer };
    layout_vertical_stack(title.GetParent()->GetRect(), windows_with_footer, layout_with_footer);
}

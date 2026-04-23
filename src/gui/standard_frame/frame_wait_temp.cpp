#include "frame_wait_temp.hpp"

#include <cstdio>

#include "auto_layout.hpp"

namespace {
static constexpr std::array layout {
    StackLayoutItem { .height = StackLayoutItem::stretch, .margin_side = 16, .margin_top = 16 },
    StackLayoutItem { .height = StackLayoutItem::stretch, .margin_side = 16 },
    standard_stack_layout::for_radio,
};
} // namespace

FrameWaitTemp::FrameWaitTemp(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &text)
    : text_custom(parent, {}, is_multiline::yes, is_closed_on_click_t::no, text)
    , text_temp(parent, {})
    , radio(parent, {}, fsm_phase) {

    text_custom.SetAlignment(Align_t::Center());

    text_temp.set_font(GuiDefaults::FontBig);
    text_temp.SetAlignment(Align_t::CenterTop());
    text_temp.SetBlinkColor(COLOR_AZURE);

    parent->CaptureNormalWindow(radio);

    std::array<window_t *, layout.size()> windows { &text_custom, &text_temp, &radio };
    layout_vertical_stack(parent->GetRect(), windows, layout);
}

void FrameWaitTemp::set_temperature(int temperature) {
    snprintf(text_temp_buffer.data(), text_temp_buffer.size(), "%3d °C", temperature);
    text_temp.SetText(string_view_utf8::MakeRAM(text_temp_buffer.data()));
    text_temp.Invalidate();
}

void FrameWaitTemp::update(fsm::PhaseData data) {
    set_temperature(((data[0] << 8) | data[1]) % 1000);
}

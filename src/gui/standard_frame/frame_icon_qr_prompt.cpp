#include "frame_icon_qr_prompt.hpp"

#if HAS_LARGE_DISPLAY()
namespace {
constexpr int16_t gap_qr_scan = 4; // small gap between the QR code and "Scan me!"
constexpr int16_t qr_bottom_trim = 14; // the QR is top-anchored and doesn't fill its square rect; trim the empty bottom so the image sits right above "Scan me!"
} // namespace
#endif

FrameIconQRPrompt::FrameIconQRPrompt(window_frame_t *parent, FSMAndPhase fsm_phase, ErrCode err_code, [[maybe_unused]] const img::Resource *icon_res)
    : FrameQRPrompt(parent, fsm_phase, err_code)
#if HAS_LARGE_DISPLAY()
    , icon(&inner_frame, {}, icon_res)
#endif
{
#if HAS_LARGE_DISPLAY()
    icon.SetAlignment(Align_t::Center());

    // Right column, bottom-up: "Scan me!" at the very bottom, the QR just above it,
    // and the icon centered in whatever space is left above the QR.
    const Rect16 frame = inner_frame.GetRect();
    const Rect16 qr_r = qr.GetRect();
    const Rect16 scan_r = scan_me.GetRect();

    const int16_t col_left = qr_r.Left();
    const int16_t col_w = qr_r.Width();
    const int16_t col_top = frame.Top();
    const int16_t col_bottom = frame.Top() + frame.Height();
    const int16_t scan_h = scan_r.Height();

    // "Scan me!" pinned to the bottom of the column.
    scan_me.SetRect(Rect16(col_left, col_bottom - scan_h, col_w, scan_h));

    // QR just above it; its rect bottom is trimmed so the image sits close to "Scan me!".
    const int16_t qr_h = qr_r.Height() - qr_bottom_trim;
    const int16_t qr_top = (col_bottom - scan_h - gap_qr_scan) - qr_h;
    qr.SetRect(Rect16(col_left, qr_top, col_w, qr_h));

    // Icon centered in the remaining space above the QR.
    icon.SetRect(Rect16(col_left, col_top, col_w, qr_top - col_top));
#else
    // Don't show the icon on small displays - the QR column has no spare room.
#endif
}

#pragma once

#include "frame_qr_prompt.hpp"

#include <window_icon.hpp>
#include <guiconfig/guiconfig.h>

/**
 * FrameQRPrompt with an icon shown above the QR code (in the right column).
 * Used for warnings, where each warning type can carry its own icon.
 *
 * @note The icon is shown on large displays only; on small displays the QR
 *       column has no spare room, so it is omitted entirely.
 */
class FrameIconQRPrompt : public FrameQRPrompt {
public:
    FrameIconQRPrompt(window_frame_t *parent, FSMAndPhase fsm_phase, ErrCode err_code, const img::Resource *icon_res);

#if HAS_LARGE_DISPLAY()
protected:
    window_icon_t icon;
#endif
};

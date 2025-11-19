/// \file
#pragma once

#include "IDialogMarlin.hpp"
#include "string_view_utf8.hpp"
#include "window_text.hpp"
#include "window_icon.hpp"
#include <inplace_function.hpp>

#include <gui/standard_frame/frame_wait.hpp>

class window_dlg_wait_t : public IDialogMarlin {
    FrameWait frame;

public:
    window_dlg_wait_t(Rect16 rect, const string_view_utf8 &text = {});

    window_dlg_wait_t(fsm::BaseData data);

    window_dlg_wait_t(const string_view_utf8 &text)
        : window_dlg_wait_t(GuiDefaults::DialogFrameRect, text) {}

    /// Shows the dialog and blocks the UI thread until all gcodes are finished
    /// Does this in a somewhat smart way that doesn't obstruct warnings
    static void wait_for_gcodes_to_finish();

    /// Displays a waiting dialog and blocks until @p until_condition returns true
    static void wait_until(const string_view_utf8 &second_string, const stdext::inplace_function<bool()> &until_f);

    virtual void Change(fsm::BaseData) override;
};

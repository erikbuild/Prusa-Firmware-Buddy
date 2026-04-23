/// \file
#pragma once

#include <array>

#include <client_response.hpp>
#include <common/fsm_base_types.hpp>
#include <radio_button_fsm.hpp>
#include <window_frame.hpp>
#include <window_text.hpp>

/**
 * Standard layout frame.
 * Contains:
 * - Centered message text
 * - Big centered temperature reading
 * - A FSM radio
 *
 * When used as a FrameDefinition, the current temperature is pushed via the
 * first two bytes of fsm::PhaseData (big-endian uint16 in degrees Celsius).
 */
class FrameWaitTemp {

public:
    FrameWaitTemp(window_frame_t *parent, FSMAndPhase fsm_phase, const string_view_utf8 &text);

public:
    inline void set_text(const string_view_utf8 &text) {
        text_custom.SetText(text);
    }

    void set_temperature(int temperature);

    void update(fsm::PhaseData data);

protected:
    window_text_t text_custom;
    WindowBlinkingText text_temp;
    RadioButtonFSM radio;
    std::array<char, sizeof("NNN °C")> text_temp_buffer {};
};

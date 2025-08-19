#pragma once
#include "gui.hpp"
#include "screen.hpp"
#include <guiconfig/guiconfig.h>

class screen_splash_data_t : public screen_t {
    window_text_t text_progress;
    window_numberless_progress_t progress;

    char text_progress_buffer[32];

public:
    screen_splash_data_t();
    ~screen_splash_data_t();

private:
    virtual void draw() override;

protected:
    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};

/// @file
#pragma once

#include <screen_menu.hpp>
#include <WindowMenuItems.hpp>
#include <MItem_tools.hpp>

using ScreenMenuWastebin_ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    MI_NOZZLE_CLEANER_FILL,
    MI_NOZZLE_CLEANER_EMPTY_WASTEBIN,
    MI_NOZZLE_CLEANER_CAPACITY,
    MI_NOZZLE_CLEANER_AUTOPAUSE>;

class ScreenMenuWastebin : public ScreenMenuWastebin_ {
public:
    ScreenMenuWastebin()
        : ScreenMenuWastebin_(_("WASTEBIN")) {}
};

/**
 * @file screen_menu_version_info_non_mini.hpp
 */

#pragma once

#include "MItem_menus.hpp"
#include "MItem_tools.hpp"
#include "screen_menu.hpp"
#include "WindowMenuItems.hpp"
#include <guiconfig/GuiDefaults.hpp>

using ScreenMenuVersionInfo__ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, MI_INFO_FW, MI_INFO_BOOTLOADER, MI_INFO_MMU, MI_BOARD_INFO>;

class ScreenMenuVersionInfo : public ScreenMenuVersionInfo__ {
    void set_serial_number(WiInfo<28> &item, const char *sn, uint8_t bom_id);

public:
    constexpr static const char *label = N_("VERSION INFO");
    ScreenMenuVersionInfo();
};

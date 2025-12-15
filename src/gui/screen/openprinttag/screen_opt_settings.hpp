/// @file
#pragma once

#include <ScreenFactory.hpp>
#include <i_window_menu_item.hpp>

namespace buddy::openprinttag {

/// Menu item that opens an OpenPrintTag settings screen
class MI_OPT_SETTINGS final : public IWindowMenuItem {
public:
    MI_OPT_SETTINGS();
    void click(IWindowMenu &) override;
};

}; // namespace buddy::openprinttag

/// @file
#pragma once

#include <ScreenFactory.hpp>
#include <i_window_menu_item.hpp>

namespace buddy::openprinttag {

/// Menu item that opens OpenPrintTag tag list
/// That is a screen that allows reading info for tags associated with tools
class MI_OPT_TAG_LIST final : public IWindowMenuItem {
public:
    MI_OPT_TAG_LIST();
    void click(IWindowMenu &) override;
};

}; // namespace buddy::openprinttag

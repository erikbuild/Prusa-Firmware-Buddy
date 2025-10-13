/// @file
#pragma once

#include <guiconfig/GuiDefaults.hpp>
#include <MItem_tools.hpp>
#include <option/xbuddy_extension_variant_standard.h>
#include <screen_menu.hpp>
#include <WindowItemFanLabel.hpp>
#include <WindowMenuItems.hpp>

#if XBUDDY_EXTENSION_VARIANT_STANDARD()
    #include <gui/menu_item/specific/menu_items_xbuddy_extension.hpp>
#endif

class MI_INFO_PRINT_FAN : public WI_FAN_LABEL_t {
public:
    MI_INFO_PRINT_FAN();
};

class MI_INFO_HBR_FAN : public WI_FAN_LABEL_t {
public:
    MI_INFO_HBR_FAN();
};

using ScreenMenuFanInfo_ = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
    MI_INFO_PRINT_FAN,
    MI_INFO_HBR_FAN,
#if XBUDDY_EXTENSION_VARIANT_STANDARD()
    MI_INFO_XBUDDY_EXTENSION_FAN1,
    MI_INFO_XBUDDY_EXTENSION_FAN2,
    MI_INFO_XBUDDY_EXTENSION_FAN3,
#endif
    MI_ALWAYS_HIDDEN>;

class ScreenMenuFanInfo final : public ScreenMenuFanInfo_ {
public:
    ScreenMenuFanInfo();
};

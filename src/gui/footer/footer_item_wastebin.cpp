/**
 * @file footer_item_wastebin.cpp
 */

#include "footer_item_wastebin.hpp"
#include "img_resources.hpp"
#include <guiconfig/GuiDefaults.hpp>
#include <utils/color.hpp>
#include <feature/wastebin_watcher/wastebin_watcher.hpp>

FooterItemWastebin::FooterItemWastebin(window_t *parent)
    : FooterIconText_IntVal(parent, &img::wastebin_16x16, static_makeView, static_readValue) {
}

changed_t FooterItemWastebin::updateValue() {
    const changed_t changed = FooterIconText_IntVal::updateValue();
    // Warn (orange) once the wastebin is over 80 % full.
    text.SetTextColor(value > 80 ? COLOR_ORANGE : GuiDefaults::ColorText);
    return changed;
}

int FooterItemWastebin::static_readValue() {
    const uint32_t capacity = WastebinWatcher::instance().capacity();
    if (capacity == 0) {
        return 0;
    }
    return static_cast<int>(WastebinWatcher::instance().fill_level() * 100 / capacity);
}

string_view_utf8 FooterItemWastebin::static_makeView(int value) {
    static char buff[8];
    snprintf(buff, sizeof(buff), "%d%%", value);
    return string_view_utf8::MakeRAM(buff);
}

/**
 * @file footer_item_multitool.cpp
 */

#include "footer_item_multitool.hpp"
#include "marlin_client.hpp"
#include "img_resources.hpp"
#include "i18n.h"
#include "string_builder.hpp"
#include <device/board.h>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

FooterItemFinda::FooterItemFinda(window_t *parent)
    : FooterIconText_IntVal(parent, &img::finda_16x16, static_makeView, static_readValue) {
}

int FooterItemFinda::static_readValue() {
    return int(marlin_vars().mmu2_finda);
}

string_view_utf8 FooterItemFinda::static_makeView(int value) {
    //@@TODO there is a strange comment in FooterItemFSensor::static_makeView about the last character not being rendered
    // Not sure why but using the same workaround.
    // Another funny thing is that the LED in FINDA shows the exact opposite - this needs to be discussed with Content ;)
    return _(value ? N_("ON ") : N_("OFF "));
}

FooterItemCurrentTool::FooterItemCurrentTool(window_t *parent)
    : FooterIconText_IntVal(parent, &img::spool_16x16, static_makeView, static_readValue) {
}

int FooterItemCurrentTool::static_readValue() {
    return match(
        marlin_vars().active_extruder.get(),
        [](VirtualToolIndex virtual_tool) { return virtual_tool.display_index(); },
        [](NoTool) -> uint8_t { return 0; });
}

string_view_utf8 FooterItemCurrentTool::static_makeView(int value) {
    static std::array<char, 3> buff;
    StringBuilder b(buff);
    if (std::holds_alternative<VirtualToolIndex>(VirtualToolIndex::currently_selected())) {
        b.append_float(value, { .max_decimal_places = 0, .skip_zero_before_dot = false });
    } else {
        b.append_char('-'); // No filament loaded
    }
    return string_view_utf8::MakeRAM(buff.data());
}

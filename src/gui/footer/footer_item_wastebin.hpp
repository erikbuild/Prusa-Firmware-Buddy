/**
 * @file footer_item_wastebin.hpp
 * @brief footer item displaying the INDX nozzle-cleaner wastebin fill (% of capacity)
 */

#pragma once
#include "ifooter_item.hpp"

class FooterItemWastebin final : public FooterIconText_IntVal {
    static string_view_utf8 static_makeView(int value);
    static int static_readValue();

    changed_t updateValue() override;

public:
    FooterItemWastebin(window_t *parent);
};

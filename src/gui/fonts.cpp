/**
 * @file fonts.cpp
 */

#include "fonts.hpp"
#include "config.h"
#include <bsod/bsod.h>
#include <guiconfig/guiconfig.h>
#include <printers.h>
#include <option/enable_translation_ja.h>
#include <option/enable_translation_uk.h>

#if PRINTER_IS_PRUSA_MINI()
    #if ENABLE_TRANSLATION_JA()
        #include "res/cc/font_regular_7x13_latin_and_katakana.hpp" //Font::small
        #include "res/cc/font_regular_11x18_latin_and_katakana.hpp" //Font::normal
        #include "res/cc/font_regular_9x16_latin_and_katakana.hpp" //Font::special
    #elif ENABLE_TRANSLATION_UK()
        #include "res/cc/font_regular_7x13_latin_and_cyrillic.hpp" //Font::small
        #include "res/cc/font_regular_11x18_latin_and_cyrillic.hpp" //Font::normal
        #include "res/cc/font_regular_9x16_latin_and_cyrillic.hpp" //Font::special
    #else
        #include "res/cc/font_regular_7x13_latin.hpp" //Font::small
        #include "res/cc/font_regular_11x18_latin.hpp" //Font::normal
        #include "res/cc/font_regular_9x16_latin.hpp" //Font::special
    #endif
#else
    #include "res/cc/font_regular_9x16_full.hpp" //Font::small
    #include "res/cc/font_bold_11x19_full.hpp" //Font::normal
    #include "res/cc/font_bold_13x22_full.hpp" //Font::big
    #include "res/cc/font_bold_30x53_digits.hpp" //Font::large
#endif

#if PRINTER_IS_PRUSA_MINI()
static_assert(resource_font_size(Font::small) == font_size_t { font_regular_7x13.w, font_regular_7x13.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::normal) == font_size_t { font_regular_11x18.w, font_regular_11x18.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::big) == font_size_t { font_regular_11x18.w, font_regular_11x18.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::special) == font_size_t { font_regular_9x16.w, font_regular_9x16.h }, "Font size doesn't match");
#else
static_assert(resource_font_size(Font::small) == font_size_t { font_regular_9x16.w, font_regular_9x16.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::normal) == font_size_t { font_bold_11x19.w, font_bold_11x19.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::big) == font_size_t { font_bold_13x22.w, font_bold_13x22.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::special) == font_size_t { font_regular_9x16.w, font_regular_9x16.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::large) == font_size_t { font_bold_30x53.w, font_bold_30x53.h }, "Font size doesn't match");
#endif

const font_t *resource_font(Font font) {
    switch (font) {
#if PRINTER_IS_PRUSA_MINI()
    case Font::small:
        return &font_regular_7x13;
    case Font::normal:
        return &font_regular_11x18;
    case Font::big:
        return &font_regular_11x18;
    case Font::special:
        return &font_regular_9x16;
#else
    case Font::small:
        return &font_regular_9x16;
    case Font::normal:
        return &font_bold_11x19;
    case Font::big:
        return &font_bold_13x22;
    case Font::special:
        return &font_regular_9x16;
    case Font::large:
        return &font_bold_30x53;
#endif
    }
    bsod_unreachable();
}

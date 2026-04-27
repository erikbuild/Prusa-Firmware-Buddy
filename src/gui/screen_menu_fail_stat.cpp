/**
 * @file screen_menu_fail_stat.cpp
 */

#include "screen_menu_fail_stat.hpp"
#include <option/has_indx.h>

#if HAS_INDX()
ScreenMenuIndxDiag::ScreenMenuIndxDiag()
    : ScreenMenuIndxDiag__(_(label)) {
}

MI_INDX_DIAG::MI_INDX_DIAG()
    : MI_SCREEN_BASE(ScreenFactory::Screen<ScreenMenuIndxDiag>, "INDX Diagnostics", nullptr, is_hidden_t::dev) {
}
#endif

ScreenMenuFailStat::ScreenMenuFailStat()
    : ScreenMenuFailStat__(_(label)) {
}

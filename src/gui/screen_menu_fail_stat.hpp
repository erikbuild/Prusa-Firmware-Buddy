/**
 * @file screen_menu_fail_stat.hpp
 */

#pragma once

#include "screen_menu.hpp"
#include "MItem_crash.hpp"
#include "MItem_mmu.hpp"
#include "MItem_menus.hpp"
#include <option/has_mmu2.h>
#include <option/has_indx.h>

#if HAS_INDX()
    #include "MItem_tools.hpp"

using ScreenMenuIndxDiag__ = ScreenMenu<EFooter::On, MI_RETURN,
    MI_INFO_INDX_FIFO_ERR, MI_INFO_INDX_REFRESH_ERR,
    MI_INFO_XEXT_REFRESH_ERR>;

class ScreenMenuIndxDiag : public ScreenMenuIndxDiag__ {
    static constexpr const char *label = N_("INDX DIAGNOSTICS");

public:
    ScreenMenuIndxDiag();
};

class MI_INDX_DIAG : public MI_SCREEN_BASE {
public:
    MI_INDX_DIAG();
};
#endif

using ScreenMenuFailStat__ = ScreenMenu<EFooter::On, MI_RETURN
#if ENABLED(POWER_PANIC)
    ,
    MI_POWER_PANICS /*filament runout,*/
#endif // ENABLED(POWER_PANIC)
#if ENABLED(CRASH_RECOVERY)
    ,
    MI_CRASHES_X_LAST, MI_CRASHES_Y_LAST, MI_CRASHES_X, MI_CRASHES_Y
#endif // ENABLED(CRASH_RECOVERY)
#if HAS_MMU2()
    ,
    MI_MMU_LOAD_FAILS, MI_MMU_TOTAL_LOAD_FAILS, MI_MMU_GENERAL_FAILS, MI_MMU_TOTAL_GENERAL_FAILS
#endif
#if HAS_INDX()
    ,
    MI_INFO_INDX_PICKUP_FAIL, MI_INFO_INDX_PARK_FAIL,
    MI_INDX_DIAG
#endif
    >;

class ScreenMenuFailStat : public ScreenMenuFailStat__ {
    static constexpr const char *label = N_("FAILURE STATISTICS");

public:
    ScreenMenuFailStat();
};

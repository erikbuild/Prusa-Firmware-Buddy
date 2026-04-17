#include "screen_toolhead_settings_dock.hpp"
#include <option/has_dwarf.h>
#include <option/has_indx.h>

using namespace screen_toolhead_settings;

static constexpr NumericInputConfig dock_position_config_x {
    .min_value = X_MIN_POS,
    .max_value = X_MAX_POS,
    .step = 0.1f,
    .max_decimal_places = 2,
    .unit = Unit::millimeter,
};

static constexpr NumericInputConfig dock_position_config_y {
    .min_value = Y_MIN_POS,
    .max_value = Y_MAX_POS,
    .step = 0.1f,
    .max_decimal_places = 2,
    .unit = Unit::millimeter,
};

// * MI_DOCK_X
MI_DOCK_X::MI_DOCK_X(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_SPIN(toolhead, 0, dock_position_config_x, _("Dock X")) {
    update();
}

#if HAS_INDX()
// getTool was removed from INDX's toolchanger API as it only creates unnecessary layer of abstraction
static PhysicalToolIndex get_tool(PhysicalToolIndex tool) { return tool; } // Trivial pass-through on INDX
#elif HAS_DWARF()
static buddy::puppies::Dwarf &get_tool(PhysicalToolIndex tool) { return prusa_toolchanger.getTool(tool); }
#endif

float MI_DOCK_X::read_value_impl(PhysicalToolIndex ix) {
    return prusa_toolchanger.get_tool_info(get_tool(ix)).dock_x;
}

void MI_DOCK_X::store_value_impl(PhysicalToolIndex ix, float set) {
    PrusaToolInfo info = prusa_toolchanger.get_tool_info(get_tool(ix));
    info.dock_x = set;
    prusa_toolchanger.set_tool_info(get_tool(ix), info);
    prusa_toolchanger.save_tool_info();
}

// * MI_DOCK_Y
MI_DOCK_Y::MI_DOCK_Y(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_SPIN(toolhead, 0, dock_position_config_y, _("Dock Y")) {
    update();
}

float MI_DOCK_Y::read_value_impl(PhysicalToolIndex ix) {
    return prusa_toolchanger.get_tool_info(get_tool(ix)).dock_y;
}

void MI_DOCK_Y::store_value_impl(PhysicalToolIndex ix, float set) {
    PrusaToolInfo info = prusa_toolchanger.get_tool_info(get_tool(ix));
    info.dock_y = set;
    prusa_toolchanger.set_tool_info(get_tool(ix), info);
    prusa_toolchanger.save_tool_info();
}

#if HAS_SELFTEST()
// * MI_DOCK_CALIBRATE
MI_DOCK_CALIBRATE::MI_DOCK_CALIBRATE(Toolhead toolhead)
    : MI_TOOLHEAD_SPECIFIC_BASE(toolhead, _("Calibrate Dock Position")) {
}

void MI_DOCK_CALIBRATE::click(IWindowMenu &) {
    marlin_client::test_start_with_data(stmDocks, std::get<PhysicalToolIndex>(toolhead()));
}
#endif

// * ScreenToolheadDetailDock
ScreenToolheadDetailDock::ScreenToolheadDetailDock(Toolhead toolhead)
    : ScreenMenu(_("DOCK CONFIGURATION"))
    , toolhead(toolhead) //
{
    menu_set_toolhead(container, toolhead);
}

#include "MItem_print.hpp"
#include "marlin_client.hpp"
#include "common/conversions.hpp"
#include "menu_vars.h"
#include "WindowMenuSpin.hpp"
#include "img_resources.hpp"
#include <numeric_input_config_common.hpp>
#include <utils/string_builder.hpp>
#include <option/has_mmu2.h>

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include "module/prusa/toolchanger.h"
#endif
#if HAS_MMU2()
    #include <feature/prusa/MMU2/mmu2_mk4.h>
#endif

/*****************************************************************************/
// MI_NOZZLE_ABSTRACT
is_hidden_t MI_NOZZLE_ABSTRACT::is_hidden([[maybe_unused]] uint8_t tool_nr) {
    const auto tool = stdext::get_optional<PhysicalToolIndex>(PhysicalToolIndex::from_raw_notool(tool_nr));
    if (tool.has_value() && tool->is_enabled()) {
        return is_hidden_t::no;
    }
    return is_hidden_t::yes;
}

MI_NOZZLE_ABSTRACT::MI_NOZZLE_ABSTRACT(uint8_t tool_nr, [[maybe_unused]] const char *label)
    : WiSpin(uint16_t(marlin_vars().hotend(tool_nr).target_nozzle), numeric_input_config::nozzle_temperature,
#if HAS_TOOLCHANGER()
        prusa_toolchanger.is_toolchanger_enabled() ? _(label) : _(generic_label),
#else
        _(generic_label),
#endif
        &img::nozzle_16x16, is_enabled_t::yes, is_hidden(tool_nr))
    , tool_nr(tool_nr) {
}

void MI_NOZZLE_ABSTRACT::OnClick() {
    marlin_client::set_target_nozzle(static_cast<int16_t>(value()), tool_nr);
}

/*****************************************************************************/
// MI_INFO_NOZZLE_TEMP
MI_INFO_NOZZLE_TEMP::MI_INFO_NOZZLE_TEMP(std::variant<PhysicalToolIndex, CurrentlySelectedTool> tool)
    : MenuItemAutoUpdatingLabel(string_view_utf8 {}, standard_print_format::temp_c,
        [](auto *item) { return static_cast<MI_INFO_NOZZLE_TEMP *>(item)->value(); })
    , tool_(tool) {
    SetLabel(match(
        tool_,
        [&](PhysicalToolIndex t) { return t.display_name(label_params_); },
        [](CurrentlySelectedTool) { return _("Nozzle Temp"); }));
}

float MI_INFO_NOZZLE_TEMP::value() const {
    const auto tool = resolve_enabled_tool_index(tool_);
    if (!tool.has_value()) {
        return NAN;
    }

    return marlin_vars().hotend(*tool).temp_nozzle;
}

/*****************************************************************************/
// MI_INFO_HEATBREAK_TEMP
MI_INFO_HEATBREAK_TEMP::MI_INFO_HEATBREAK_TEMP(std::variant<PhysicalToolIndex, CurrentlySelectedTool> tool)
    : MenuItemAutoUpdatingLabel(string_view_utf8 {}, standard_print_format::temp_c,
        [](auto *item) { return static_cast<MI_INFO_HEATBREAK_TEMP *>(item)->value(); })
    , tool_(tool) {
    SetLabel(match(
        tool_,
        [&](PhysicalToolIndex t) { return t.display_name(label_params_); },
        [](CurrentlySelectedTool) { return _("Heatbreak Temp"); }));
}

float MI_INFO_HEATBREAK_TEMP::value() const {
    const auto tool = resolve_enabled_tool_index(tool_);
    if (!tool.has_value()) {
        return NAN;
    }

    return marlin_vars().hotend(*tool).temp_heatbreak;
}

/*****************************************************************************/
// MI_HEATBED

MI_HEATBED::MI_HEATBED()
    : WiSpin(uint8_t(marlin_vars().target_bed), numeric_input_config::bed_temperature, _(label), &img::heatbed_16x16, is_enabled_t::yes, is_hidden_t::no) {
}
void MI_HEATBED::OnClick() {
    marlin_client::set_target_bed(static_cast<int16_t>(value()));
}

/*****************************************************************************/
// MI_PRINTFAN

static constexpr NumericInputConfig printfan_spin_config = {
    .max_value = 100,
    .special_value = 0,
    .unit = Unit::percent,
};

MI_PRINTFAN::MI_PRINTFAN()
    : WiSpin(val_mapping(false, marlin_vars().print_fan_speed, 255, 100),
        printfan_spin_config, _(label), &img::fan_16x16, is_enabled_t::yes, is_hidden_t::no) {
}
void MI_PRINTFAN::OnClick() {
    marlin_client::set_fan_speed(val_mapping(true, static_cast<uint8_t>(value()), 100, 255));
}

/*****************************************************************************/
// MI_SPEED

static constexpr NumericInputConfig print_speed_spin_config = {
    .min_value = 10,
    .max_value = 300,
    .unit = Unit::percent,
};

MI_SPEED::MI_SPEED()
    : WiSpin(uint16_t(marlin_vars().print_speed), print_speed_spin_config, _(label), &img::speed_16x16, is_enabled_t::yes, is_hidden_t::no) {
}

void MI_SPEED::OnClick() {
    marlin_client::set_print_speed(static_cast<uint16_t>(value()));
}

/*****************************************************************************/
// MI_FLOWFACT_ABSTRACT
is_hidden_t MI_FLOWFACT_ABSTRACT::is_hidden([[maybe_unused]] uint8_t tool_nr) {
    const auto tool = stdext::get_optional<VirtualToolIndex>(VirtualToolIndex::from_raw_notool(tool_nr));
    if (tool.has_value() && tool->is_enabled()) {
        return is_hidden_t::no;
    }
    return is_hidden_t::yes;
}

static constexpr NumericInputConfig flowfact_spin_config {
    .min_value = 50,
    .max_value = 150,
    .unit = Unit::percent,
};

MI_FLOWFACT_ABSTRACT::MI_FLOWFACT_ABSTRACT(uint8_t tool_nr, [[maybe_unused]] const char *label)
    : WiSpin(uint16_t(marlin_vars().hotend(tool_nr).flow_factor), flowfact_spin_config,
#if HAS_TOOLCHANGER()
        prusa_toolchanger.is_toolchanger_enabled() ? _(label) : _(generic_label),
#elif HAS_MMU2()
        MMU2::mmu2.Enabled() ? _(label) : _(generic_label),
#else
        _(generic_label),
#endif
        nullptr, is_enabled_t::yes, is_hidden(tool_nr))
    , tool_nr(tool_nr) {
}

void MI_FLOWFACT_ABSTRACT::OnClick() {
    marlin_client::set_flow_factor(static_cast<uint16_t>(value()), tool_nr);
}

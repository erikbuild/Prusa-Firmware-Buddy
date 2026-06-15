/**
 * @file footer_eeprom.cpp
 * @author Radek Vana
 * @date 2021-05-20
 */

#include "footer_eeprom.hpp"
#include <config_store/store_instance.hpp>
#include <utility_extensions.hpp>

namespace footer::eeprom {

Record stored_settings_as_record() {
    return config_store().footer_setting.get_all();
}

namespace {
    /**
     * @brief load draw configuration from eeprom
     *        check validity
     *        store valid configuration if was invalid
     * @return ItemDrawCnf
     */
    ItemDrawCnf load_and_validate_draw_cnf() {
        ItemDrawCnf cnf = ItemDrawCnf(config_store().footer_draw_type.get());
        uint32_t valid = uint32_t(cnf);
        // cannot use Set - would recursively call this function
        config_store().footer_draw_type.set(valid);
        return cnf;
    }

    ItemDrawCnf &get_draw_cnf_ref() {
        static ItemDrawCnf type = load_and_validate_draw_cnf();
        return type;
    }
} // namespace

ItemDrawCnf load_item_draw_cnf() {
    return get_draw_cnf_ref();
}

changed_t set(ItemDrawCnf cnf) {
    if (get_draw_cnf_ref() == cnf) {
        return changed_t::no;
    }
    config_store().footer_draw_type.set(static_cast<uint32_t>(cnf));
    get_draw_cnf_ref() = cnf;
    return changed_t::yes;
}

changed_t set(ItemDrawType type) {
    ItemDrawCnf cnf = get_draw_cnf_ref();
    cnf.type = type;
    return set(cnf);
}

changed_t set(draw_zero_t zero) {
    ItemDrawCnf cnf = get_draw_cnf_ref();
    cnf.zero = zero;
    return set(cnf);
}

changed_t set_center_n_and_fewer(uint8_t center_n_and_fewer) {
    ItemDrawCnf cnf = get_draw_cnf_ref();
    cnf.center_n_and_fewer = center_n_and_fewer;
    return set(cnf);
}

ItemDrawType get_item_draw_type() {
    return get_draw_cnf_ref().type;
}

draw_zero_t get_item_draw_zero() {
    return get_draw_cnf_ref().zero;
}

uint8_t get_center_n_and_fewer() {
    return get_draw_cnf_ref().center_n_and_fewer;
}

/// This is just a check to make it harder to corrupt eeprom
static_assert(std::to_underlying(Item::none) == 0
        && std::to_underlying(Item::nozzle) == 1
        && std::to_underlying(Item::bed) == 2
        && std::to_underlying(Item::filament) == 3
        && std::to_underlying(Item::f_s_value) == 4
        && std::to_underlying(Item::f_sensor) == 5
        && std::to_underlying(Item::speed) == 6
        && std::to_underlying(Item::axis_x) == 7
        && std::to_underlying(Item::axis_y) == 8
        && std::to_underlying(Item::axis_z) == 9
        && std::to_underlying(Item::z_height) == 10
        && std::to_underlying(Item::print_fan) == 11
        && std::to_underlying(Item::heatbreak_fan) == 12
        && std::to_underlying(Item::input_shaper_x) == 13
        && std::to_underlying(Item::input_shaper_y) == 14
        && std::to_underlying(Item::live_z) == 15
        && std::to_underlying(Item::heatbreak_temp) == 16
        && std::to_underlying(Item::sheets) == 17
        && std::to_underlying(Item::finda) == 18
        && std::to_underlying(Item::current_tool) == 19
        && std::to_underlying(Item::all_nozzles) == 20
        && std::to_underlying(Item::f_sensor_side) == 21
        && std::to_underlying(Item::nozzle_diameter) == 22
        && std::to_underlying(Item::nozzle_pwm) == 23
        && std::to_underlying(Item::chamber_temp) == 24
        && std::to_underlying(Item::f_s_value_side) == 25
        && std::to_underlying(Item::wastebin_pellets) == 26
        && true, // So that we don't have to move the ',' around
    "Numbers assigned to items should never change and always be available (not ifdefed)!!");

static_assert(std::to_underlying(Item::_count) == 27, "When adding a new item, increment this counter and add it to the static_assert above");

} // namespace footer::eeprom

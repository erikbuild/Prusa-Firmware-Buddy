#include "data_utils.hpp"

namespace buddy::openprinttag {

AmountsInfo::AmountsInfo(const RequestRef &req) {
    if (!req.are_results_valid()) {
        // Cannot reliably process data - failing to read a specific fields might yield unwanted results
        return;
    }

    if (auto val = req.result<MainField::actual_netto_full_weight>()) {
        full_weight_g = *val;

    } else if (auto val = req.result<MainField::nominal_netto_full_weight>()) {
        full_weight_g = *val;
    }

    if (auto val = req.result<AuxField::consumed_weight>(); full_weight_g.has_value()) {
        remaining_weight_g = *full_weight_g - val.value_or(0);
    }
}

AbbreviationInfo::AbbreviationInfo(const RequestRef &req) {
    if (!req.are_results_valid()) {
        // Cannot reliably process data - failing to read a specific fields might yield unwanted results
        return;
    }

    if (auto abbr = req.result<MainField::material_abbreviation>()) {
        abbreviation = std::string_view { abbreviation_buffer.begin(), abbr->copy(abbreviation_buffer.begin(), abbreviation_buffer.size()) };

    } else if (const auto type = req.result<MainField::material_type>()) {
        abbreviation = ::openprinttag::enum_item_name(*type);
    }
}

FilamentParametersInfo::FilamentParametersInfo(const RequestRef &req) {
    using Params = FilamentTypeParameters;

    // Mark all parameters as missing, we will clear that as we get them
    missing_parameters = ~MissingParameters {};

    // We need to fill something valid otherwise we could produce invalid parameters if everything failed
    parameters.name = "XXX";

    if (!req.are_results_valid()) {
        // Cannot reliably process data - failing to read a specific fields might yield unwanted results
        return;
    }

    const auto set = [&]<auto Params::*mem_ptr>(auto value) {
        parameters.*mem_ptr = value;
        missing_parameters.set(filament_type_parameter_index<mem_ptr>, false);
    };

    const auto load_to = [&]<auto field, auto Params::*mem_ptr>() {
        if (auto val = req.result<field>()) {
            set.operator()<mem_ptr>(*val);
        }
    };

    /// @deprecated set.operator() should be used, because it clears the missing_parameters bitset when setting
    auto &parameters_unsafe = parameters;

    /// Shadow the parameters variable so that noone uses it accidentally
    /// If you want to set a parameter, use the @p set function that also clears missing_parameters
    [[maybe_unused]] const auto &parameters = parameters_unsafe;

    static_assert(filament_type_parameter_count == 6 + HAS_CHAMBER_API() * 4 + HAS_FILAMENT_HEATBREAK_PARAM() * 1 + HAS_FILAMENT_BASE_PRESET_PARAM() * 1, "We probably need to implement something here");

    // Abbreviation
    {
        AbbreviationInfo abbreviation { req };

        auto base_type = FilamentType::from_name(abbreviation.abbreviation);

        // Necessary compatibility workaround. OPT uses actual chemistries for abbreviations,
        // while the Buddy is stuck with the "FLEX" preset name for TPU
        if (base_type == FilamentType::none && abbreviation.abbreviation == "TPU") {
            base_type = PresetFilamentType::FLEX;
        }

        // Use preset parameters as a basis to prefill/suggest missing data
        if (base_type != FilamentType::none) {
            parameters_unsafe = base_type.parameters();

#if HAS_FILAMENT_BASE_PRESET_PARAM()
            if (auto base_preset = std::get_if<PresetFilamentType>(&base_type)) {
                set.operator()<&Params::base_preset>(*base_preset);
            }
#endif
        }

        // Check validity of the abbreviation
        if (FilamentType { PendingAdHocFilamentType {} }.can_be_renamed_to(abbreviation.abbreviation)) {
            set.operator()<&Params::name>(abbreviation.abbreviation);

        } else {
            // Set the name, but do not clear the missing flag
            parameters_unsafe.name = abbreviation.abbreviation;
        }
    }

    // Nozzle temp
    {
        stdext::inplace_vector<int16_t, 2> nozzle_temps;
        if (auto val = req.result<MainField::min_print_temperature>()) {
            nozzle_temps.push_back(*val);
        }
        if (auto val = req.result<MainField::max_print_temperature>()) {
            nozzle_temps.push_back(*val);
        }

        if (!nozzle_temps.empty()) {
            // Pick average of the temps
            const auto temp = std::ranges::fold_left(nozzle_temps, 0, std::plus {}) / nozzle_temps.size();
            set.operator()<&Params::nozzle_temperature>(temp);
        }
    }

    // Bed temp
    {
        stdext::inplace_vector<int16_t, 2> bed_temps;
        if (auto val = req.result<MainField::min_bed_temperature>()) {
            bed_temps.push_back(*val);
        }
        if (auto val = req.result<MainField::max_bed_temperature>()) {
            bed_temps.push_back(*val);
        }

        if (!bed_temps.empty()) {
            // Pick average from the temps
            const auto temp = std::ranges::fold_left(bed_temps, 0, std::plus {}) / bed_temps.size();
            set.operator()<&Params::heatbed_temperature>(temp);
        }
    }

    load_to.operator()<MainField::preheat_temperature, &Params::nozzle_preheat_temperature>();

#if HAS_CHAMBER_API()
    load_to.operator()<MainField::chamber_temperature, &Params::chamber_target_temperature>();
    load_to.operator()<MainField::min_chamber_temperature, &Params::chamber_min_temperature>();
    load_to.operator()<MainField::max_chamber_temperature, &Params::chamber_max_temperature>();
#endif

    // Tags
    using Tag = ::openprinttag::MaterialTag;
    const auto tags = req.result<MainField::tags>().value_or(std::span<Tag> {});
    for (Tag tag : tags) {
        switch (tag) {

        case Tag::abrasive:
            set.operator()<&Params::is_abrasive>(true);
            break;

#if HAS_CHAMBER_API()
        case Tag::filtration_recommended:
            set.operator()<&Params::requires_filtration>(true);
            break;
#endif

        default:
            break;
        }
    }

    bool is_flexible = false;

    // Do not auto retract flexible materials
    if (auto val = req.result<MainField::shore_hardness_a>(); val.has_value() && *val < 95) {
        is_flexible = true;

    } else if (auto val = req.result<MainField::shore_hardness_d>(); val.has_value() && *val < 50) {
        is_flexible = true;
    }

    if (is_flexible) {
        set.operator()<&Params::is_flexible>(true);
    }

    const auto unset_missing = [&]<auto Params::*mem_ptr>() {
        missing_parameters.set(filament_type_parameter_index<mem_ptr>, false);
    };

    const auto unset_missing_if_equals = [&]<auto Params::*mem_ptr>(auto default_val) {
        if (parameters_unsafe.*mem_ptr == default_val) {
            unset_missing.operator()<mem_ptr>();
        }
    };

    // If these values are "missing" but are at their default values (based on FilamentTypePreset),
    // it likely means that them not being present just means that they are on their defaults.

    // But for example in our printer presets, ASA is set as "requires filtration",
    // so if requires_filtration == true from the preset and the tag did not indicate it, the missing flag is kept
    unset_missing_if_equals.operator()<&Params::is_abrasive>(false);
    unset_missing_if_equals.operator()<&Params::is_flexible>(false);
    unset_missing_if_equals.operator()<&Params::requires_filtration>(false);

    if (is_missing<&FilamentTypeParameters::chamber_min_temperature>() && is_missing<&FilamentTypeParameters::chamber_max_temperature>()) {
        // Chamber target temperature is not required if neither chamber_min_temperature or chamber_max_temperature are present
        unset_missing.operator()<&FilamentTypeParameters::chamber_target_temperature>();
    }

    // Chamber min max parameters are never required
    unset_missing.operator()<&FilamentTypeParameters::chamber_min_temperature>();
    unset_missing.operator()<&FilamentTypeParameters::chamber_max_temperature>();
}

} // namespace buddy::openprinttag

/// @file
#include "indx_filament_params.hpp"

#include <utils/enum_array.hpp>

#include <filament.hpp>

namespace indx {

namespace {

    constexpr FilamentParameters fallback_parameters {
        .heat_per_mm = 4.5f,
        .heat_time_constant = 0.95f,
    };

    constexpr FilamentParameters no_filament_parameters {
        .heat_per_mm = 0,
        // Very low  time constant -> heat changes should apply immediately
        .heat_time_constant = 0.0001f,
    };

    constexpr EnumArray<PresetFilamentType, FilamentParameters, PresetFilamentType::_count> preset_parameters {
        {
            PresetFilamentType::PLA,
            FilamentParameters {
                .heat_per_mm = 6.1f,
                .heat_time_constant = 1.5f,
            },
        },
        {
            PresetFilamentType::PETG,
            FilamentParameters {
                .heat_per_mm = 4.3f,
                .heat_time_constant = 0.87f,
            },
        },
        {
            PresetFilamentType::ASA,
            FilamentParameters {
                .heat_per_mm = 4.0f,
                .heat_time_constant = 0.91f,
            },
        },
        {
            PresetFilamentType::PC,
            FilamentParameters {
                .heat_per_mm = 4.4f,
                .heat_time_constant = 0.91f,
            },
        },
        {
            PresetFilamentType::PVB,
            FilamentParameters {
                .heat_per_mm = 4.5f,
                .heat_time_constant = 0.99f,
            },
        },
        {
            PresetFilamentType::ABS,
            FilamentParameters {
                .heat_per_mm = 3.9f,
                .heat_time_constant = 0.89f,
            },
        },
        {
            PresetFilamentType::HIPS,
            FilamentParameters {
                .heat_per_mm = 3.9f,
                .heat_time_constant = 0.96f,
            },
        },
        {
            PresetFilamentType::PP,
            FilamentParameters {
                .heat_per_mm = 5.1f,
                .heat_time_constant = 1.25f,
            },
        },
        {
            PresetFilamentType::FLEX,
            FilamentParameters {
                .heat_per_mm = 5.9f,
                .heat_time_constant = 1.17f,
            },
        },
        {
            PresetFilamentType::PA,
            FilamentParameters {
                .heat_per_mm = 5.2f,
                .heat_time_constant = 0.86f,
            },
        },
    };

} // namespace

const FilamentParameters &FilamentParameters::for_filament(const FilamentTypeParameters &filament_parameters) {
    if (auto preset = filament_parameters.base_preset) {
        return preset_parameters[*preset];
    } else {
        return fallback_parameters;
    }
}

const FilamentParameters &FilamentParameters::for_no_filament() {
    return no_filament_parameters;
}

} // namespace indx

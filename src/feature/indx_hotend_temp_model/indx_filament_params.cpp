/// @file
#include "indx_filament_params.hpp"

#include <utils/enum_array.hpp>

#include <filament.hpp>

namespace indx {

namespace {

    constexpr FilamentParameters fallback_parameters {
        .heat_per_mm = 4.5f,
    };

    constexpr FilamentParameters no_filament_parameters {
        .heat_per_mm = 0,
    };

    constexpr EnumArray<PresetFilamentType, FilamentParameters, PresetFilamentType::_count> preset_parameters {
        {
            PresetFilamentType::PLA,
            FilamentParameters {
                .heat_per_mm = 6.1f,
            },
        },
        {
            PresetFilamentType::PETG,
            FilamentParameters {
                .heat_per_mm = 4.3f,
            },
        },
        {
            PresetFilamentType::ASA,
            FilamentParameters {
                .heat_per_mm = 4.0f,
            },
        },
        {
            PresetFilamentType::PC,
            FilamentParameters {
                .heat_per_mm = 4.4f,
            },
        },
        {
            PresetFilamentType::PVB,
            FilamentParameters {
                .heat_per_mm = 4.5f,
            },
        },
        {
            PresetFilamentType::ABS,
            FilamentParameters {
                .heat_per_mm = 3.9f,
            },
        },
        {
            PresetFilamentType::HIPS,
            FilamentParameters {
                .heat_per_mm = 3.9f,
            },
        },
        {
            PresetFilamentType::PP,
            FilamentParameters {
                .heat_per_mm = 5.1f,
            },
        },
        {
            PresetFilamentType::FLEX,
            FilamentParameters {
                .heat_per_mm = 5.9f,
            },
        },
        {
            PresetFilamentType::PA,
            FilamentParameters {
                .heat_per_mm = 5.2f,
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

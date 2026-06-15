/// @file
#pragma once

struct FilamentTypeParameters;

namespace indx {

/// See table in BFW-8630 for the computation algo.
struct FilamentParameters {
    /// Something to do with heat transfer.
    float heat_per_mm;

    /// Thermal time constant of filament, in seconds.
    /// Calculated as time to transfer (1-1/e) of heat into 2mm diameter cylinder. Inversely proportional to thermal diffusivity of the material.
    float heat_time_constant;

    /// Implemented in indx_filament_params.cpp
    static const FilamentParameters &for_filament(const FilamentTypeParameters &filament_parameters);

    static const FilamentParameters &for_no_filament();
};

} // namespace indx

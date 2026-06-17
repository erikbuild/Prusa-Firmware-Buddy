/// @file
#pragma once

struct FilamentTypeParameters;

namespace indx {

/// See table in BFW-8630 for the computation algo.
struct FilamentParameters {
    /// Heat capacity per unit length [J / °C * m]
    float linear_heat_capacity_J_C_m;

    /// Implemented in indx_filament_params.cpp
    static const FilamentParameters &for_filament(const FilamentTypeParameters &filament_parameters);

    static const FilamentParameters &for_no_filament();

    bool operator==(const FilamentParameters &) const = default;
};

} // namespace indx

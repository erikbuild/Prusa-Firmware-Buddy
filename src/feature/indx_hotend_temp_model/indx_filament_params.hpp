/// @file
#pragma once

struct FilamentTypeParameters;

namespace indx {

/// See table in BFW-8630 for the computation algo.
struct FilamentParameters {
    /// Something to do with heat transfer.
    float heat_per_mm;

    /// Implemented in indx_filament_params.cpp
    static const FilamentParameters &for_filament(const FilamentTypeParameters &filament_parameters);

    static const FilamentParameters &for_no_filament();

    bool operator==(const FilamentParameters &) const = default;
};

} // namespace indx

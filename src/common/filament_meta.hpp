/// @file
#pragma once

#include <cstdlib>

#include <filament.hpp>
#include <common/visit_all_struct_fields.hpp>

static constexpr uint8_t filament_type_parameter_count = aggregate_arity<FilamentTypeParameters>();

template <auto FilamentTypeParameters::*mem_ptr>
static constexpr uint8_t filament_type_parameter_index = [] {
    size_t i = 0;
    size_t result = filament_type_parameter_count;
    FilamentTypeParameters params;
    visit_all_struct_fields(params, [&](const auto &field) {
        if constexpr (requires { &field == &(params.*mem_ptr); }) {
            if (&field == &(params.*mem_ptr)) {
                result = i;
            }
        }
        i++;
    });

    if (result == filament_type_parameter_count) {
        // Compile-time, we can't do anything nicer
        std::abort();
    }

    return result;
}();

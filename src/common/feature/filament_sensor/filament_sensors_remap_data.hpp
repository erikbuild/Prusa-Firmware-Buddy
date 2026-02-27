#pragma once

#include <array>
#include <cstdint>
#include <tool_index.hpp>

namespace side_fsensor_remap {

/// Maximum number of sensor mapping slots. Do not change without config store migration.
inline constexpr size_t max_sensor_mapping_count = 6;
static_assert(PhysicalToolIndex::count <= max_sensor_mapping_count);

/**
 * @note Mapping of tools to sensor positions.
 *   mapping[extruder] = sensor_position
 * @warning This type is used in config store. Changing its size requires a config store migration.
 */
using Mapping = std::array<uint8_t, max_sensor_mapping_count>;

namespace preset {
    template <size_t... Is>
    constexpr std::array<uint8_t, sizeof...(Is)> generate_normal_mapping(std::index_sequence<Is...>) {
        //  this is just fancy template way to init array in initializer_list
        return { (Is)... };
    }

    /**
     * @brief Default layout when not remapped is a plain integer sequence.
     * @warning This value is used in config store and should not be changed.
     */
    inline constexpr Mapping no_mapping { generate_normal_mapping(std::make_index_sequence<std::tuple_size<Mapping> {}>()) };

} // namespace preset

} // namespace side_fsensor_remap

#pragma once

#include <filament.hpp>

namespace config_store_ns::migrations {

/// Convert raw bytes from the old variant-based FilamentType EEPROM representation.
///
/// In GCC's libstdc++ on ARM 32-bit (the firmware target), std::variant stores
/// the union data in the base class and the index in the derived class, giving
/// a [value_byte, discriminant_byte] layout where discriminant is the 0-based
/// alternative index.
FilamentType filament_type_from_variant_bytes(uint8_t discriminant, uint8_t value);

} // namespace config_store_ns::migrations

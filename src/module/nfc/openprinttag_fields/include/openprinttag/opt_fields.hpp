/// @file
#pragma once

#include <openprinttag/opt_defines.hpp>

namespace openprinttag {

#include <openprinttag/autogen/fields.hpp.in>

constexpr inline Section field_section(MetaField) {
    return Section::meta;
}
constexpr inline Section field_section(MainField) {
    return Section::main;
}
constexpr inline Section field_section(AuxField) {
    return Section::auxiliary;
}

} // namespace openprinttag

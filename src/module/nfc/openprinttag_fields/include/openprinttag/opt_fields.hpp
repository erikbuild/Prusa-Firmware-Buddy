/// @file
#pragma once

#include <array>
#include <string_view>
#include <utility>

#include <openprinttag/opt_defines.hpp>
#include <openprinttag/util_defines.hpp>

namespace openprinttag {

enum class FieldType {
    bytes,
    enum_,
    enum_array,
    int_,
    number,
    string,
    timestamp,
    uuid,
    color_rgba,
};

template <auto field>
struct FieldMeta;

template <typename Enum>
constexpr inline size_t enum_capacity() = delete;

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

template <typename FieldEnum>
constexpr inline TagField tag_field(TagID tag, FieldEnum field) {
    return TagField {
        .tag = tag,
        .section = field_section(field),
        .field = static_cast<Field>(field)
    };
}

} // namespace openprinttag

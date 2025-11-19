#include "ndef.hpp"

#include <cstdlib>
#include <cstring>
#include <bit>

using namespace openprinttag;

static_assert(sizeof(NDEFRecordStaticHeader) == 2);
static_assert(sizeof(NDEFRecordFullHeader) == 7);

PayloadPos NDEFRecordFullHeader::dynamic_field_data_offset(DynamicField field) const {
    PayloadPos result = 0;

    if (field == DynamicField::payload_length) {
        return result;
    }
    result += (is_payload_length_1b ? 1 : 4);

    if (field == DynamicField::id_length) {
        return result;
    }
    result += (has_id ? 1 : 0);

    if (field == DynamicField::type) {
        return result;
    }
    result += type_length;

    if (field == DynamicField::id) {
        return result;
    }
    result += dynamic_field_length(DynamicField::id);

    if (field == DynamicField::payload) {
        return result;
    }
    // result += dynamic_field_length(DynamicField::payload);

    // Unreachable
    std::abort();
}

PayloadPos NDEFRecordFullHeader::dynamic_field_length(DynamicField field) const {
    switch (field) {

    case DynamicField::payload_length:
        return is_payload_length_1b ? 1 : 4;

    case DynamicField::id_length:
        return has_id ? 1 : 0;

    case DynamicField::type:
        return type_length;

    case DynamicField::id:
        return has_id ? dynamic_data[dynamic_field_data_offset(DynamicField::id_length)] : 0;

    case DynamicField::payload: {
        const auto payload_length = dynamic_field_length(DynamicField::payload_length);
        uint32_t result = 0;
        memcpy(&result, dynamic_data /* + dynamic_field_data_offset(DynamicField::payload_length) - always 0 */, payload_length);
        if (payload_length > 1) {
            // Big endian -> little endian
            static_assert(std::endian::native == std::endian::little);
            result = std::byteswap(result);
        }
        return result;
    }
    }

    std::abort();
}

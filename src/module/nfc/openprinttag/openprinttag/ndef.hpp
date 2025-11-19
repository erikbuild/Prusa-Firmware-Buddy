#pragma once

#include "util_defines.hpp"

namespace openprinttag {

enum class NDEFTypeNameFormat : uint8_t {
    empty = 0,
    well_known = 1,
    mime_media_type = 2,
    absolute_uri = 3,
    external = 4,
    unkonwn = 5,
    unchanged = 6,
    reserved = 7,
};

enum class NDEFTLVTag : uint8_t {
    null = 0x00,
    ndef = 0x03,
    proprietary = 0xfd,
    terminator = 0xfe,
};

/// Part of the NDEF header that is static - always the same
struct NDEFRecordStaticHeader {
    NDEFTypeNameFormat type_name_format : 3;

    /// Denotes whether ID and ID length fields are present
    bool has_id : 1;

    /// True -> payload length is 1B, otherwise 4B
    bool is_payload_length_1b : 1;

    bool chunk_flag : 1 = false;
    bool message_end : 1 = false;
    bool message_begin : 1 = false;

    uint8_t type_length;
};

/// Struct representing the full NDEF header.
/// Becuse the NDEF headers have dynamic size, the actual header doesn't have to always take the whole space of this struct
struct NDEFRecordFullHeader : public NDEFRecordStaticHeader {

public:
    enum class DynamicField : uint8_t {
        payload_length,
        id_length,
        type,
        id,
        payload
    };

public:
    /// Dynamic data - can contain type_length and payload_length
    uint8_t dynamic_data[5];

public:
    /// \returns offset of the dynamic field relative to the header start
    PayloadPos dynamic_field_offset(DynamicField field) const {
        return dynamic_field_data_offset(field) + sizeof(NDEFRecordStaticHeader);
    }

    /// \returns offset of the dynamic field relative to the dynamic_data start
    PayloadPos dynamic_field_data_offset(DynamicField field) const;

    PayloadPos dynamic_field_length(DynamicField field) const;

    /// \returns span of the dynamic field relative to the header start
    PayloadSpan dynamic_field_span(DynamicField field) const {
        return PayloadSpan { .offset = dynamic_field_offset(field), .size = dynamic_field_length(field) };
    }

    /// \returns total record length, including the payload
    PayloadPos record_length() const {
        return dynamic_field_span(DynamicField::payload).end();
    }
};

} // namespace openprinttag

#include "ll_nfc_reader.hpp"

#include <cstring>

/*
 * data:
 *   aux:
 *     consumed_material: 100
 *   main:
 *     brand: Prusament
 *     is_signed: false
 *     material_class: FFF
 *     material_name: PLA Prusa Galaxy Black
 *     material_type: ASA
 *     netto_full_weight: 8589934591
 *     tags:
 *     - glitter
 *     - abrasive
 *     transmission_distance: 0.3
 *   meta:
 *     aux_region_offset: 231
 *     main_region_offset: 11
 * raw_data:
 *   aux: bf001864ff000000000000000000000000000000000000000000000000000000
 *   main: bf01000202036950727573616d656e740476504c412050727573612047616c61787920426c61636b06f4091b00000001ffffffff0ef934cd0f9f0611ffff0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
 *   meta: a2000b0218e70000000000
 * regions:
 *   aux:
 *     offset: 231
 *     size: 32
 *     used_size: 5
 *   main:
 *     offset: 11
 *     size: 220
 *     used_size: 62
 *   meta:
 *     offset: 0
 *     size: 11
 *     used_size: 6
 * root:
 *   message_size: 300
 *   overhead: 37
 *   payload_size: 263
 *   payload_used_size: 73
 *   total_used_size: 110
 *
 */
static constexpr const uint8_t default_tag_data[] {
    194, 31, 0, 0, 1, 7, 97, 112, 112, 108, 105, 99, 97, 116, 105, 111, 110, 47, 118, 110, 100, 46, 112, 114, 117, 115, 97, 51, 100, 46, 109, 97, 116, 46, 110, 102, 99, 162, 0, 11, 2, 24, 231, 0, 0, 0, 0, 0, 191, 1, 0, 2, 2, 3, 105, 80, 114, 117, 115, 97, 109, 101, 110, 116, 4, 118, 80, 76, 65, 32, 80, 114, 117, 115, 97, 32, 71, 97, 108, 97, 120, 121, 32, 66, 108, 97, 99, 107, 6, 244, 9, 27, 0, 0, 0, 1, 255, 255, 255, 255, 14, 249, 52, 205, 15, 159, 6, 17, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 24, 100, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

LLNFCReader::LLNFCReader() {
    memcpy(tag_data_, default_tag_data, sizeof(default_tag_data));
}

INFCReader::IOResult<void> LLNFCReader::read(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) {
    if (tag != 0) {
        return std::unexpected(IOError::other);
    }

    if (start + buffer.size() > tag_size_) {
        return std::unexpected(IOError::outside_of_bounds);
    }

    memcpy(buffer.data(), tag_data_ + start, buffer.size());
    return {};
}

INFCReader::IOResult<void> LLNFCReader::write(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) {
    if (tag != 0) {
        return std::unexpected(IOError::other);
    }

    if (start + buffer.size() > tag_size_) {
        return std::unexpected(IOError::outside_of_bounds);
    }

    memcpy(tag_data_ + start, buffer.data(), buffer.size());
    return {};
}

bool LLNFCReader::get_event(Event &e) {
    if (!tag_detected_reported_) {
        tag_detected_reported_ = true;
        e = TagDetectedEvent { .tag = 0 };
        return true;
    }

    return false;
}

void LLNFCReader::forget_tag(NFCTagID tag) {
    if (tag == 0) {
        tag_detected_reported_ = false;
    }
}

void LLNFCReader::reset_state() {
    tag_detected_reported_ = false;
}

#pragma once
#include <cstdio>

namespace buddy::bbf {

enum class TLVType : uint8_t {
    RESOURCES_TARBALL = 9,
    RESOURCES_TARBALL_DIGEST = 10,
    BOOTLOADER_TARBALL = 11,
    BOOTLOADER_TARBALL_DIGEST = 12,
};

bool seek_to_tlv_entry(FILE *bbf, TLVType type, uint32_t &length);

}; // namespace buddy::bbf

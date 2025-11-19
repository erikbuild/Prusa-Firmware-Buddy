#pragma once

#include <cstdint>

namespace openprinttag {

enum class Region : uint8_t {
    meta,
    main,
    auxiliary,
    _cnt,
};

/// Section and region are virtually the same
/// Region is the region of the NFC tag allocated for some data
/// Section is the actual data (so the used part of the region)
/// Optionally, section can also be followed by a signature
using Section = Region;

using Field = uint16_t;

/// Field mapping for the meta section (universal for all NFC tags)
// TODO auto-generate from the specification repo
enum class MetaField : Field {
    main_region_offset = 0,
    main_region_size = 1,
    aux_region_offset = 2,
    aux_region_size = 3,
};

} // namespace openprinttag

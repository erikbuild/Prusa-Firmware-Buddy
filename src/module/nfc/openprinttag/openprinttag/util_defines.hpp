#pragma once

#include <cstdint>
#include <algorithm>

namespace openprinttag {

/// Reader-specific identifier for a tag the reader is managing
using TagID = uint8_t;

/// One reader can have multiple independent antennas
using ReaderAntenna = uint8_t;

static constexpr TagID invalid_nfc_tag = -1;

/// NFC analog for size_t
using PayloadPos = uint16_t;

static constexpr PayloadPos invalid_nfc_offset = -1;

/// std::span analog for the OPT data
struct PayloadSpan {

public:
    static constexpr PayloadSpan from_offset_end(PayloadPos offset, PayloadPos end) {
        return PayloadSpan {
            .offset = offset,
            .size = static_cast<PayloadPos>(end - offset),
        };
    }

    PayloadPos offset = 0;
    PayloadPos size = 0;

    constexpr PayloadPos end() const {
        return offset + size;
    }

public:
    constexpr bool is_empty() const {
        return size == 0;
    }

    constexpr bool contains(const PayloadSpan &subspan) const {
        return (subspan.offset >= offset) && (subspan.end() <= end());
    }

    constexpr PayloadSpan combined(const PayloadSpan &other) {
        if (is_empty()) {
            return other;

        } else if (other.is_empty()) {
            return *this;

        } else {
            return from_offset_end(std::min(offset, other.offset), std::max(end(), other.end()));
        }
    }

    constexpr PayloadSpan added_offset(PayloadPos added_offset) {
        return PayloadSpan {
            .offset = static_cast<PayloadPos>(offset + added_offset),
            .size = size
        };
    }

    constexpr bool operator==(const PayloadSpan &) const = default;
};

struct TagPayloadSpan {

public:
    TagID tag = invalid_nfc_tag;
    PayloadSpan span;

public:
    constexpr bool contains(const TagPayloadSpan &subspan) const {
        return (tag == subspan.tag) && span.contains(subspan.span);
    }
};

} // namespace openprinttag

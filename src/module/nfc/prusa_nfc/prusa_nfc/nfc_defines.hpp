#pragma once

#include <cstdint>
#include <algorithm>

/// Identifier for a tag the NFC reader is managing
using NFCTagID = uint8_t;

/// One reader can have multiple independent antennas
using NFCAntenna = uint8_t;

static constexpr NFCTagID invalid_nfc_tag = -1;

/// NFC analog for size_t
using NFCOffset = uint16_t;

/// std::span analog for NFC data
struct NFCSpan {

public:
    static constexpr NFCSpan from_offset_end(NFCOffset offset, NFCOffset end) {
        return NFCSpan {
            .offset = offset,
            .size = static_cast<NFCOffset>(end - offset),
        };
    }

    NFCOffset offset = 0;
    NFCOffset size = 0;

    constexpr NFCOffset end() const {
        return offset + size;
    }

public:
    constexpr bool is_empty() const {
        return size == 0;
    }

    constexpr bool contains(const NFCSpan &subspan) const {
        return (subspan.offset >= offset) && (subspan.end() <= end());
    }

    constexpr NFCSpan combined(const NFCSpan &other) {
        if (is_empty()) {
            return other;

        } else if (other.is_empty()) {
            return *this;

        } else {
            return from_offset_end(std::min(offset, other.offset), std::max(end(), other.end()));
        }
    }

    constexpr bool operator==(const NFCSpan &) const = default;
};

struct NFCTagSpan {

public:
    NFCTagID tag = invalid_nfc_tag;
    NFCSpan span;

public:
    constexpr bool contains(const NFCTagSpan &subspan) const {
        return (tag == subspan.tag) && span.contains(subspan.span);
    }
};

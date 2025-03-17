#pragma once

#include <cstdint>

/// Identifier for a tag the NFC reader is managing
using NFCTagID = uint16_t;

static constexpr NFCTagID invalid_nfc_tag = -1;

/// NFC analog for size_t
using NFCOffset = uint16_t;

/// std::span analog for NFC data
struct NFCSpan {

public:
    NFCOffset offset = 0;
    NFCOffset size = 0;

    constexpr NFCOffset end() const {
        return offset + size;
    }

public:
    constexpr bool contains(const NFCSpan &subspan) const {
        return (subspan.offset >= offset) && (subspan.end() <= end());
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

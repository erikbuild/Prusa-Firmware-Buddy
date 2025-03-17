#pragma once

#include <span>
#include <variant>
#include <expected>

#include "nfc_defines.hpp"

/// Interface for low-level reading from NFC tags
class INFCReader {

public:
    /// A new tag has been detected by the reader
    struct TagDetectedEvent {
        NFCTagID tag;
    };

    /// The reader has lost connection with a tag
    struct TagLostEvent {
        NFCTagID tag;
    };

    using Event = std::variant<TagDetectedEvent, TagLostEvent>;

    enum class IOError : uint8_t {
        /// Trying to read/write outside of the tag memory
        outside_of_bounds,

        /// Other, unspecified error.
        /// Retrying the operation might help.
        other,
    };

    template <typename T>
    using IOResult = std::expected<T, IOError>;

public:
    /// Reads \p buffer.size() bytes from \p tag into \p buffer, starting at position \p start
    /// \returns whether the operation was successful
    [[nodiscard]] virtual IOResult<void> read(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) = 0;

    /// Writes \p buffer.size() bytes to \p tag, starting at position \p start
    /// \returns whether the operation was successful
    [[nodiscard]] virtual IOResult<void> write(NFCTagID tag, NFCOffset start, const std::span<std::byte> &data) = 0;

    /// Reads a single event and stores it in \p e
    /// \returns false if there is no event
    [[nodiscard]] virtual bool get_event(Event &e) = 0;
};

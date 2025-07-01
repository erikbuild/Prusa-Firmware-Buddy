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

        /// Antenna the tag has been detected on.
        /// If tag gets moved from antenna to antenna, the reader will emit TagLost and new TagDetected (and likely assign a new ID to the tag)
        NFCAntenna antenna;
    };

    /// The reader has lost connection with a tag
    /// Please call \p forget_tag after processing this event to allow reuse of the tag ID
    struct TagLostEvent {
        NFCTagID tag;
    };

    using Event = std::variant<TagDetectedEvent, TagLostEvent>;

    enum class IOError : uint8_t {
        /// Trying to read/write outside of the tag memory
        outside_of_bounds,

        /// TagID is invalid (or already invalidated)
        invalid_id,

        /// Other, unspecified error.
        /// Retrying the operation might help.
        other,

        /// The operation is not implemented (possibly because of the prameter combination)
        not_implemented,
    };

    template <typename T>
    using IOResult = std::expected<T, IOError>;

public:
    /// Reads \p buffer.size() bytes from \p tag into \p buffer, starting at position \p start
    /// \returns whether the operation was successful
    [[nodiscard]] virtual IOResult<void> read(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) = 0;

    /// Writes \p buffer.size() bytes to \p tag, starting at position \p start
    /// \returns whether the operation was successful
    [[nodiscard]] virtual IOResult<void> write(NFCTagID tag, NFCOffset start, const std::span<const std::byte> &data) = 0;

    /// Reads a single event and stores it in \p e
    /// \returns false if there is no event
    [[nodiscard]] virtual bool get_event(Event &e) = 0;

    /// Completely forgets the tag and allows the tag ID to be reused
    /// If the tag is still present, a new TagDetected event will be emitted
    virtual void forget_tag(NFCTagID tag) = 0;

    virtual void reset_state() = 0;

public:
    struct InitializeTagParams {
        enum class ProtectionPolicy : uint8_t {
            /// No protection
            none = 0,

            /// Irreversibly lock the registers & memory
            lock = 1,

            /// Password-protect writing
            /// NOTE: Some registers cannot be password-protected, those will be locked
            write_password = 2,

            _cnt,
        };

        /// Password to be used for ProtectionPolicy::write_password
        uint32_t password = 0;

        /// If > 0, first N bytes of the tag will be protected according to the data protection policy
        /// The number has to be aligned to the tag block size
        NFCOffset protect_first_num_bytes = 0;

        /// Determines how the tag should be protected
        /// - Protects first \p data_protection_size bytes of data
        /// - Protects utility registers such as AFI, DSFID, EAS and so on
        ProtectionPolicy protection_policy = ProtectionPolicy::none;

        /// Do not fail immediately on error, try locking/initializing whatever possible
        bool best_effort = false;
    };

    /// Initializes the tag according to the recommended practices:
    /// - Sets up utility registers such as AFI, DSFID, EAS, ...
    /// - (optionally) Locks the registers
    /// - (optionally) Locks the headers data
    /// Might be implemented only for specific chip models and specific parameter configurations
    /// All init-time data should be written to the tag beforehand using \p raw_write
    [[nodiscard]] virtual IOResult<void> initialize_tag(NFCTagID tag, const InitializeTagParams &params) {
        (void)tag, (void)params;
        return std::unexpected(IOError::not_implemented);
    }

    /// Removes write protection for the memory protected by the specified write password
    /// !!! This does not fully undo the password protection, nor does it change the password
    /// !!! Registers and other things will still be protected
    [[nodiscard]] virtual IOResult<void> unlock_tag(NFCTagID tag, uint32_t password) {
        (void)tag, (void)password;
        return std::unexpected(IOError::not_implemented);
    }
};

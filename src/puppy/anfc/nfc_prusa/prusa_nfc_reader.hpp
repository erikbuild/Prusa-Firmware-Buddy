#pragma once

#include <expected>
#include <span>
#include <string_view>

#include <inplace_vector.hpp>
#include <inplace_function.hpp>
#include <utils/enum_array.hpp>

#include <utils/cache.hpp>
#include <nfc_ll/nfc_defines.hpp>
#include <nfc_ll/i_nfc_reader.hpp>

#include "prusa_nfc_defines.hpp"

/// Higher-level NFC interface, working with the Prusa Material NFC format
class PrusaNFCReader {

public:
    /// Maximum size of tag we support (have buffers for)
    static constexpr size_t max_tag_size = 512;

    static constexpr size_t max_meta_region_size = 32;

    static constexpr size_t metadata_cache_capacity = 4;

    struct Params {
        std::string_view mime_type = "application/vnd.prusa3d.mat.nfc";

        /// Whether the tag has a meta region.
        /// False means the the payload only contains the main section (no aux, no meta)
        bool has_meta_region : 1 = true;
    };

    struct NFCTagField {
        NFCTagID tag;
        NFCSection section;
        NFCField field;
    };

    enum class Error : uint8_t {
        /// The field or region is not present on the tag (but the tag is otherwise OK)
        field_not_present,

        /// The field type in the CBOR is not readable with the used function (for example trying to read float using read_field_bool)
        wrong_field_type,

        /// Cannot write to the region, because it is write protected (can be protected on various levels)
        write_protected,

        /// The section we are trying to read from is corrupted.
        /// Re-reading the section won't help, but clear_section() could.
        region_corrupt,

        /// The tag has been determined to be not a valid Prusa NFC tag (wrong mime, corrupted contents, ...)
        /// Retrying won't help, the chip needs to be re-formatted.
        tag_invalid,

        /// Data is too big and doesn't fit somewhere (tag itself, internal buffers, ...)
        data_too_big,

        /// Other, unspecified error (comm error, ...)
        /// Retrying the operation might help.
        other,
    };

    static Error to_prusa_error(INFCReader::IOError error);

    template <typename T>
    using IOResult = std::expected<T, Error>;

    using Event = INFCReader::Event;

    struct RegionMetadata {
        /// Region span, from the start of the NFC tag
        /// ! Warning - in the meta region CBOR, the offsets are relative to the NDEF payload start
        NFCSpan span;

        inline bool is_present() const {
            return span.size != 0;
        }
    };

    struct TagMetadata {
        EnumArray<NFCRegion, RegionMetadata, 3> region;

        /// Stores whether the tag is a valid Prusa tag
        bool is_valid : 1 = false;

        const RegionMetadata &meta_region() const {
            return region[NFCRegion::meta];
        }
        const RegionMetadata &main_region() const {
            return region[NFCRegion::main];
        }
        const RegionMetadata &aux_region() const {
            return region[NFCRegion::auxiliary];
        }
    };

    struct WriteReport {
        /// Whether the field existed before the write
        bool field_existed;

        /// Anything changed at all by the operation
        bool changed;

        bool operator==(const WriteReport &) const = default;
    };

public:
    PrusaNFCReader(INFCReader &reader);

    const Params &params() const {
        return params_;
    }

    /// Configures the reader with the specified parameters
    void set_params(const Params &set);

    /// See INFCReader::get_event
    [[nodiscard]] bool get_event(Event &event);

    /// Invalidates all the cache records the reader has for the specified tag
    /// To be called if the tag gets somehow changed outside of the PrusaNFCReader control
    void invalidate_cache(NFCTagID tag);

public:
    /// \returns metadata for the tag. Employs the cache if possible
    [[nodiscard]] IOResult<const TagMetadata *> read_metadata(NFCTagID tag);

    /// Enumerates fields of section \param section in \param tag and stores the list of the fields into \param result.
    /// \returns Number of fields stored in \p result
    [[nodiscard]] IOResult<std::span<const NFCField>> enumerate_fields(NFCTagID tag, NFCRegion section, const std::span<NFCField> &result);

    /// Removes the specified field from the tag.
    [[nodiscard]] IOResult<WriteReport> remove_field(const NFCTagField &field);

    [[nodiscard]] IOResult<bool> read_field_bool(const NFCTagField &field);
    [[nodiscard]] IOResult<int32_t> read_field_int32(const NFCTagField &field);
    [[nodiscard]] IOResult<int64_t> read_field_int64(const NFCTagField &field);
    [[nodiscard]] IOResult<float> read_field_float(const NFCTagField &field);
    [[nodiscard]] IOResult<std::string_view> read_field_string(const NFCTagField &field, const std::span<char> &buffer);
    [[nodiscard]] IOResult<std::span<const uint16_t>> read_field_uint16_array(const NFCTagField &field, const std::span<uint16_t> &buffer);

    [[nodiscard]] IOResult<WriteReport> write_field_bool(const NFCTagField &field, bool value);
    [[nodiscard]] IOResult<WriteReport> write_field_int32(const NFCTagField &field, int32_t value);
    [[nodiscard]] IOResult<WriteReport> write_field_int64(const NFCTagField &field, int64_t value);
    [[nodiscard]] IOResult<WriteReport> write_field_float(const NFCTagField &field, float value);
    [[nodiscard]] IOResult<WriteReport> write_field_string(const NFCTagField &field, const std::string_view &value);
    [[nodiscard]] IOResult<WriteReport> write_field_uint16_array(const NFCTagField &field, const std::span<const uint16_t> &value);

private: // Reading internal functions
    /// Read the specified span from the NFC tag and stores it into the read buffer
    /// \returns the read data (might not be aligned with the read buffer)
    [[nodiscard]] IOResult<std::span<std::byte>> read_span(const NFCTagSpan &span);

    /// Defined in the .cpp
    template <typename T, auto f>
    [[nodiscard]] inline IOResult<T> read_field_primitive(const NFCTagField &field);

    /// Defined in the .cpp, to hide the cbor library
    struct CBORValue;

    struct ContinueEnumerating {};

    struct StopEnumerating {};

    using EnumerateCallbackResult = std::variant<ContinueEnumerating, StopEnumerating, Error, int>;

    /// The callback is expected to read or skip every value
    /// \returns result of cbor_read or cbor_skip
    using EnumerateCallback = stdext::inplace_function<EnumerateCallbackResult(NFCField field, CBORValue v)>;

    /// Calls \p callback for each field in the \p section of \p tag
    /// \returns size of the read CBOR object (if the iteration was not stopped using StopEnumerating)
    [[nodiscard]] IOResult<NFCOffset> enumerate_fields_callback(NFCTagID tag, NFCSection section, const EnumerateCallback &callback);

    /// Calls \p callback for each fielkd in the section at \p span
    /// \returns size of the read CBOR object (if the iteration was not stopped using StopEnumerating)
    [[nodiscard]] IOResult<NFCOffset> enumerate_fields_callback(const NFCTagSpan &span, const EnumerateCallback &callback);

    using ReadFieldCallbackResult = std::variant<int, Error>;

    using ReadFieldCallback = stdext::inplace_function<ReadFieldCallbackResult(CBORValue v)>;

    /// Calls \p callback over CBOR \p field
    IOResult<void> read_field_impl(const NFCTagField &field, const ReadFieldCallback &callback);

private: // Writing internal functions
    /// Defined in the .cpp, to hide the cbor library
    struct CBOREncoder;

    using WriteFieldCallback = stdext::inplace_function<int(CBOREncoder e)>;

    /// Writes the specified field on the tag.
    /// \param callback is called for writing the field value. If not set, the function will remove the field instead.
    IOResult<WriteReport> write_field_impl(const NFCTagField &field, const WriteFieldCallback &callback);

private:
    /// Lower-level reader that we're using to communicate with the NFC
    INFCReader &reader_;

private:
    /// Buffer for NFC reading operations
    std::array<std::byte, max_tag_size> read_buffer_;

    /// Buffer for NFC writing operations
    std::array<std::byte, max_tag_size> write_buffer_;

    /// What is currently stored in the read buffer
    NFCTagSpan read_bufer_span_;

    /// Cache of basic NFC tag information, so that we don't have to parse it for every access
    Cache<NFCTagID, TagMetadata, metadata_cache_capacity> metadata_cache_;

private:
    Params params_;
};

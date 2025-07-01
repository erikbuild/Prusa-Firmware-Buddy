#pragma once

#include <utils/atomic_circular_queue.hpp>
#include <prusa_nfc/i_nfc_reader.hpp>
#include <nfcv/types.hpp>
#include <nfcv/rw_interface.hpp>
#include <inplace_vector.hpp>
#include <inplace_function.hpp>

class LLNFCReader final : public INFCReader {
public:
    static constexpr size_t MAX_KNOWN_TAGS = 8;

    LLNFCReader(nfcv::ReaderWriterInterface &reader);

    [[nodiscard]] IOResult<void> read(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) final;

    [[nodiscard]] IOResult<void> write(NFCTagID tag, NFCOffset start, const std::span<const std::byte> &buffer) final;

    [[nodiscard]] bool get_event(Event &e) final;

    void forget_tag(NFCTagID tag) final;

    void reset_state() final;

    [[nodiscard]] virtual IOResult<void> initialize_tag(NFCTagID tag, const InitializeTagParams &params) override;

    [[nodiscard]] virtual IOResult<void> unlock_tag(NFCTagID tag, uint32_t password) override;

private:
    enum class TagType : uint8_t {
        slix2,
        unknown,
    };
    struct TagData {
        enum class State : uint8_t {
            free,
            known,
            lost
        };

        nfcv::UID uid;
        nfcv::ReaderWriterInterface::AntennaData antenna;
        uint8_t block_size;
        uint8_t block_count;
        State state = State::free;
        TagType tag_type;
    };

    nfcv::ReaderWriterInterface &reader;
    std::array<TagData, MAX_KNOWN_TAGS> tags {};

    AtomicCircularQueue<Event, uint8_t, 4> events;

    /// Runs a next discovery procedure, where we try to find new tags, or detect tags that are gone.
    void run_next_discovery();

    /// Validates if tag_id is referencing existing NFC tag (eather known or lost) - but the index references valid data
    [[nodiscard]] bool is_valid(NFCTagID tag_id);

    [[nodiscard]] std::unexpected<IOError> handle_io_error(NFCTagID tag, nfcv::Error error);

    using IOOpFunc = nfcv::Result<void>(const TagData &tag_data);

    /// Helper to intialize the reader for next io operation
    /// TODO: With mutiple readers this should return what reader was used
    [[nodiscard]] IOResult<void> io_op(NFCTagID tag, NFCOffset start, size_t buffer_size, const stdext::inplace_function<IOOpFunc> &impl);

    /// Helper function for write implementation
    [[nodiscard]] nfcv::Result<void> write_impl(const TagData &tag_data, NFCOffset start, const std::span<const std::byte> &buffer);

    /// Helper function for read implementation
    [[nodiscard]] nfcv::Result<void> read_impl(const TagData &tag_data, NFCOffset start, const std::span<std::byte> &buffer);
};

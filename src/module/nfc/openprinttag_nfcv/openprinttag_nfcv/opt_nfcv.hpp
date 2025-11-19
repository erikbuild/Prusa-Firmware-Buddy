#pragma once

#define DO_NOT_CHECK_ATOMIC_LOCK_FREE

#include <utils/atomic_circular_queue.hpp>
#include <utils/timing/rate_limiter.hpp>
#include <openprinttag/opt_backend.hpp>
#include <nfcv/types.hpp>
#include <nfcv/rw_interface.hpp>
#include <inplace_vector.hpp>
#include <inplace_function.hpp>

namespace openprinttag {

class OPTBackend_NFCV final : public OPTBackend {
public:
    static constexpr size_t MAX_KNOWN_TAGS = 8;
    static constexpr uint32_t PAUSE_BETWEEN_DISCOVERIES_MS = 250;

    OPTBackend_NFCV(nfcv::ReaderWriterInterface &reader, ReaderAntenna enforced_antenna = OPTBackend::no_antenna_enforce);

    [[nodiscard]] IOResult<void> read(TagID tag, PayloadPos start, const std::span<std::byte> &buffer) final;

    [[nodiscard]] IOResult<void> write(TagID tag, PayloadPos start, const std::span<const std::byte> &buffer) final;

    [[nodiscard]] bool get_event(Event &e, uint32_t current_time_ms) final;

    [[nodiscard]] IOResult<size_t> get_tag_uid(TagID tag, const std::span<std::byte> &buffer) final;

    [[nodiscard]] virtual IOResult<void> read_tag_info(TagID tag, TagInfo &target) final;

    void forget_tag(TagID tag) final;

    void reset_state() final;

    [[nodiscard]] virtual IOResult<void> initialize_tag(TagID tag, const InitializeTagParams &params) override;

    [[nodiscard]] virtual IOResult<void> unlock_tag(TagID tag, uint32_t password) override;

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
        nfcv::ReaderWriterInterface::AntennaID antenna;
        uint8_t block_size;
        uint8_t block_count;
        State state = State::free;
        TagType tag_type;
    };

    nfcv::ReaderWriterInterface &reader;
    std::array<TagData, MAX_KNOWN_TAGS> tags {};

    RateLimiter<uint32_t> discoveries_limiter { PAUSE_BETWEEN_DISCOVERIES_MS };
    nfcv::ReaderWriterInterface::AntennaID discovery_antenna = 0;

    AtomicCircularQueue<Event, uint8_t, 4> events;

    /// Runs a next discovery procedure, where we try to find new tags, or detect tags that are gone.
    void run_next_discovery();

    /// Validates if tag_id is referencing existing NFC tag (eather known or lost) - but the index references valid data
    [[nodiscard]] bool is_valid(TagID tag_id);

    [[nodiscard]] std::unexpected<IOError> handle_io_error(TagID tag, nfcv::Error error);

    using IOOpFunc = nfcv::Result<void>(const TagData &tag_data);

    /// Helper to intialize the reader for next io operation
    /// TODO: With mutiple readers this should return what reader was used
    [[nodiscard]] IOResult<void> io_op(TagID tag, PayloadPos start, size_t buffer_size, const stdext::inplace_function<IOOpFunc> &impl);

    /// Helper function for write implementation
    [[nodiscard]] nfcv::Result<void> write_impl(const TagData &tag_data, PayloadPos start, const std::span<const std::byte> &buffer);

    /// Helper function for read implementation
    [[nodiscard]] nfcv::Result<void> read_impl(const TagData &tag_data, PayloadPos start, const std::span<std::byte> &buffer);
};

} // namespace openprinttag

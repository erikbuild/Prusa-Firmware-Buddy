#pragma once

#include <openprinttag/opt_backend.hpp>

namespace openprinttag {

// !!! Currently stub implementation for testing
class OPTBackend_Stub final : public OPTBackend {

public:
    OPTBackend_Stub();

    [[nodiscard]] IOResult<void> read(TagID tag, PayloadPos start, const std::span<std::byte> &buffer) final;

    [[nodiscard]] IOResult<void> write(TagID tag, PayloadPos start, const std::span<const std::byte> &buffer) final;

    [[nodiscard]] bool get_event(Event &e, uint32_t current_time_ms) final;

    [[nodiscard]] IOResult<size_t> get_tag_uid(TagID tag, const std::span<std::byte> &buffer) final;

    [[nodiscard]] virtual IOResult<void> read_tag_info(TagID tag, TagInfo &target) final;

    void forget_tag(TagID tag) final;

    void reset_state() final;

private:
    static constexpr size_t tag_size_ = 320;
    std::byte tag_data_[tag_size_];
    bool tag_detected_reported_ = false;
};

} // namespace openprinttag

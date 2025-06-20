#pragma once

#include <prusa_nfc/i_nfc_reader.hpp>

// !!! Currently stub implementation for testing
class LLNFCReader final : public INFCReader {

public:
    LLNFCReader();

    [[nodiscard]] IOResult<void> read(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) final;

    [[nodiscard]] IOResult<void> write(NFCTagID tag, NFCOffset start, const std::span<const std::byte> &buffer) final;

    [[nodiscard]] bool get_event(Event &e) final;

    void forget_tag(NFCTagID tag) final;

    void reset_state() final;

private:
    static constexpr size_t tag_size_ = 300;
    std::byte tag_data_[tag_size_];
    bool tag_detected_reported_ = false;
};

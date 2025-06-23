#include "ll_nfc_reader.hpp"

#include <bitset>
#include <raii/scope_guard.hpp>

LLNFCReader::LLNFCReader(nfcv::ReaderWriterInterface &reader)
    : reader(reader) {
    reset_state();
}

INFCReader::IOResult<void> LLNFCReader::io_op(NFCTagID tag, NFCOffset start, size_t buffer_size, stdext::inplace_function<IOOpFunc> impl) {
    if (!is_valid(tag)) {
        return std::unexpected(IOError::invalid_id);
    }
    const auto &tag_data = tags.at(tag);

    if (const auto end = start + buffer_size; end > tag_data.block_count * tag_data.block_size) {
        return std::unexpected(IOError::outside_of_bounds);
    }

    if (!reader.field_up(tag_data.antenna).has_value()) {
        return std::unexpected(IOError::other);
    }
    ScopeGuard auto_field_down = [this]() {
        reader.field_down();
    };

    if (const auto impl_res = impl(tag_data); !impl_res.has_value()) {
        if (impl_res.error() == nfcv::Error::no_response) {
            // Lets see if this is going to work, if it is too eager, then we can maybe set some timer and check if it happens too often
            if (!events.enqueue(INFCReader::TagLostEvent { .tag = tag })) {
                std::abort();
            }
            return {};
        }
        return std::unexpected(IOError::other);
    }

    return {};
}

INFCReader::IOResult<void> LLNFCReader::read(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) {
    // Default lambda capture doesn't fit the stdext::inplace_function so we need a helper to pass the data correctly
    struct {
        NFCOffset start;
        const std::span<std::byte> &buffer;
    } ref = { .start = start, .buffer = buffer };
    return io_op(tag, start, buffer.size(), [this, &ref](const TagData &tag_data) {
        return read_impl(tag_data, ref.start, ref.buffer);
    });
}

nfcv::Result<void> LLNFCReader::read_impl(const TagData &tag_data, NFCOffset start, const std::span<std::byte> &buffer) {
    auto block_index = start / tag_data.block_size;
    auto it = buffer.begin();
    int32_t in_block_byte_offset = start % tag_data.block_size;
    std::array<std::byte, nfcv::MAX_BLOCK_SIZE_IN_BYTES> tmp_buf {};
    for (; it != buffer.end(); ++block_index) {
        if (const auto res = reader.read_single_block(tag_data.uid, block_index, std::span { tmp_buf.data(), tag_data.block_size }); !res.has_value()) {
            return res;
        }

        const int32_t remaining_bytes = std::distance(it, buffer.end());
        const auto to_copy = std::min(tag_data.block_size - in_block_byte_offset, remaining_bytes);
        auto tmp_buf_it = tmp_buf.begin();
        it = std::copy_n(std::next(tmp_buf_it, in_block_byte_offset), to_copy, it);
        in_block_byte_offset = 0;
    }

    return {};
}

INFCReader::IOResult<void> LLNFCReader::write(NFCTagID tag, NFCOffset start, const std::span<const std::byte> &buffer) {
    // Default lambda capture doesn't fit the stdext::inplace_function so we need a helper to pass the data correctly
    struct {
        NFCOffset start;
        const std::span<const std::byte> &buffer;
    } ref = { .start = start, .buffer = buffer };
    return io_op(tag, start, buffer.size(), [this, &ref](const TagData &tag_data) { return write_impl(tag_data, ref.start, ref.buffer); });
}

nfcv::Result<void> LLNFCReader::write_impl(const TagData &tag_data, NFCOffset start, const std::span<const std::byte> &buffer) {
    std::array<std::byte, nfcv::MAX_BLOCK_SIZE_IN_BYTES> tmp_buf {};
    const std::span<std::byte> tmp_buf_block_span { tmp_buf.data(), tag_data.block_size };
    auto source_it = buffer.begin();

    auto block_index = start / tag_data.block_size;
    const auto in_block_byte_offset = start % tag_data.block_size;

    // if we are not block alligned we need to read the first block - write some data at the end of it and then write this edited block
    if (in_block_byte_offset != 0) {
        if (const auto res = reader.read_single_block(tag_data.uid, block_index, tmp_buf_block_span); !res.has_value()) {
            return res;
        }

        const auto to_copy = std::min(static_cast<size_t>(tag_data.block_size - in_block_byte_offset), buffer.size());
        std::copy_n(source_it, to_copy, std::next(tmp_buf.begin(), in_block_byte_offset));
        std::advance(source_it, to_copy);

        if (const auto res = reader.write_single_block(tag_data.uid, block_index, tmp_buf_block_span); !res.has_value()) {
            return res;
        }
        ++block_index;
    }

    // let's write the aligned data
    // TODO: use write blocks command
    for (; std::distance(source_it, buffer.end()) >= tag_data.block_size; std::advance(source_it, tag_data.block_size), ++block_index) {
        auto block_write = buffer.subspan(std::distance(buffer.begin(), source_it), tag_data.block_size);

        if (const auto res = reader.write_single_block(tag_data.uid, block_index, block_write); !res.has_value()) {
            return res;
        }
    }

    // again if the data are not block aligned at the end we need to read the block, rewrite the start of it and then write the whole block
    if (source_it != buffer.end()) {
        if (const auto res = reader.read_single_block(tag_data.uid, block_index, tmp_buf_block_span); !res.has_value()) {
            return res;
        }

        std::copy(source_it, buffer.end(), tmp_buf_block_span.begin());

        if (const auto res = reader.write_single_block(tag_data.uid, block_index, tmp_buf_block_span); !res.has_value()) {
            return res;
        }
    }

    return {};
}

bool LLNFCReader::get_event(Event &e) {
    // check for events to report
    if (events.isEmpty()) {
        // if we don't have any => run discovery to detect new tags
        run_next_discovery();
        // if still don't have any events => return false;
        if (events.isEmpty()) {
            return false;
        }
    }

    assert(!events.isEmpty());

    e = events.dequeue();
    return true;
}

void LLNFCReader::forget_tag(NFCTagID tag) {
    assert(tag < MAX_KNOWN_TAGS);
    auto &tag_data = tags.at(tag);
    tag_data.state = TagData::State::free;
}

void LLNFCReader::reset_state() {
    for (size_t i = 0; i < MAX_KNOWN_TAGS; ++i) {
        forget_tag(i);
    }
    events.clear();
}

bool LLNFCReader::is_valid(NFCTagID tag_id) {
    static_assert(!std::is_signed_v<decltype(tag_id)>);
    if (tag_id >= MAX_KNOWN_TAGS) {
        return false;
    }

    return tags.at(tag_id).state == TagData::State::known;
}

void LLNFCReader::run_next_discovery() {
    // Let's switch to next antenna before doing anything else
    // and power up the field
    auto antenna = reader.switch_to_next_discovery_atenna();
    if (!reader.field_up(antenna).has_value()) {
        std::abort();
    }
    ScopeGuard field_down_guard = [&]() { reader.field_down(); };

    // list of found uids in this procedure, we compare them in the end with known uids to detect lost tags
    std::bitset<MAX_KNOWN_TAGS> found_tags {};

    // let's do the procedure until all the tags answer to Inventory command
    while (const auto inv_res = reader.inventory()) {
        const auto uid = inv_res.value();
        assert(uid[nfcv::UID_MSB_INDEX] == nfcv::UID_MSB);

        // Prevent the tag to repeatedly answering to inventory (it will still answer to addressed commands (the rest of them))
        if (!reader.stay_quiet(uid).has_value()) {
            // The tag is probably gone - just skip it for now
            continue;
        }

        // Check if we already detected the tag
        // We don't need to match the antenna, since we don't want to report the same tag twice
        // If the tag moves from one antenna to another we need to first emit tag missing event and then we can detect it on new antenna again
        auto tag_data = std::ranges::find_if(tags, [&](const auto &tag) { return tag.state != TagData::State::free && tag.uid == uid; });
        if (tag_data != tags.end()) {
            // But mark it as found
            found_tags.set(std::distance(tags.begin(), tag_data));
            // We already know the tag, we don't need to register it
            continue;
        }

        // We found unknown tag, lets ask for more information about it
        const auto tag_info = reader.get_system_info(uid);
        if (!tag_info.has_value()) {
            // Again the tag is probably gone - just skip it for now
            continue;
        }

        // Lets find a free space for the tag info to live
        tag_data = std::ranges::find_if(tags, [&](const auto &tag) { return tag.state == TagData::State::free; });
        if (tag_data == tags.end()) {
            // If we can't fit a new tag into the storage we should skip reporting it for a time.
            // Probably we are waiting for forget tag command - until then we can't report new tags since we need free
            // valid tag_id
            continue;
        }
        NFCTagID tag_id = std::distance(tags.begin(), tag_data);

        // enqueue event to send
        if (!events.enqueue(INFCReader::TagDetectedEvent { .tag = tag_id, .antenna = static_cast<NFCAntenna>(antenna) })) {
            // we have too many events - this should never happend but lets be sure
            // we can't report the tag => so we don't want to store it
            // if this ultimately happens increase the events queue by little bit
            continue;
        }

        found_tags.set(tag_id);

        // Lets store all the information available
        static constexpr nfcv::TagInfo::MemorySize def_mem_size { .block_size = 0, .block_count = 0 };
        *tag_data = {
            .uid = uid,
            .antenna = antenna,
            .block_size = tag_info->mem_size.value_or(def_mem_size).block_size,
            .block_count = tag_info->mem_size.value_or(def_mem_size).block_count,
            .state = TagData::State::known,
        };
    }

    // Lets find a uid that we lost
    for (NFCTagID tag_id = 0; tag_id < tags.size(); ++tag_id) {
        if (is_valid(tag_id)) {
            auto &tag_data = tags.at(tag_id);
            // check if the tag was found during this procedure and check if originaly the it was found on current antenna
            if (tag_data.antenna == antenna && !found_tags.test(tag_id)) {
                // Detected lost tag - lets mark it and enqueue an event
                if (!events.enqueue(INFCReader::TagLostEvent { .tag = tag_id })) {
                    // we have too many events - this should never happend but lets be sure
                    // we can't report the lost tag => so we don't want to mark it as lost
                    // if this ultimately happens increase the events queue by little bit
                    continue;
                }
                tag_data.state = TagData::State::lost;
            }
        }
    }
}

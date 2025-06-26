#include <catch2/catch.hpp>

#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <span>
#include <ranges>

#include <test_utils/formatters.hpp>

#include <prusa_nfc/prusa_nfc_reader.hpp>
#include <prusa_nfc/i_nfc_reader.hpp>

using ByteString = std::basic_string<std::byte>;

#include <autogen/sample_data.cpp.in>
#include <autogen/field_enums.hpp>

/// Expected offset of the payload of the NDEF message - considering the standard prusa mime type
constexpr NFCOffset payload_offset = 37;

class MockNFCReader final : public INFCReader {
public:
    struct ReadLog {
        NFCTagID tag;
        size_t seq;
        size_t offset;
        size_t bytes;
    };
    struct WriteLog {
        NFCTagID tag;
        size_t seq;
        size_t offset;
        size_t bytes;
    };
    struct Log {
        std::vector<ReadLog> reads;
        std::vector<WriteLog> writes;

        size_t seq_counter = 0;
    };
    Log log;

    std::unordered_map<NFCTagID, ByteString> tag_data;

    [[nodiscard]] virtual IOResult<void> read(NFCTagID tag, NFCOffset start, const std::span<std::byte> &buffer) final {

        log.reads.push_back(ReadLog {
            .tag = tag,
            .seq = log.seq_counter++,
            .offset = start,
            .bytes = buffer.size(),
        });

        if (!tag_data.contains(tag)) {
            return std::unexpected(IOError::other);
        }

        auto &data = tag_data[tag];
        if (start + buffer.size() > data.size()) {
            return std::unexpected(IOError::outside_of_bounds);
        }

        memcpy(buffer.data(), data.data() + start, buffer.size());
        return {};
    }

    [[nodiscard]] virtual IOResult<void> write(NFCTagID tag, NFCOffset start, const std::span<const std::byte> &buffer) final {
        log.writes.push_back(WriteLog {
            .tag = tag,
            .seq = log.seq_counter++,
            .offset = start,
            .bytes = buffer.size(),
        });

        if (!tag_data.contains(tag)) {
            return std::unexpected(IOError::other);
        }

        auto &data = tag_data[tag];
        if (start + buffer.size() > data.size()) {
            return std::unexpected(IOError::outside_of_bounds);
        }

        memcpy(data.data() + start, buffer.data(), buffer.size());
        return {};
    }

    [[nodiscard]] virtual bool get_event(Event &e) final {
        return false;
    }

    virtual void forget_tag(NFCTagID tag) final {
    }

    virtual void reset_state() final {
    }
};

NFCSection section_for_field(NFCMetaField) {
    return NFCSection::meta;
}
NFCSection section_for_field(MainField) {
    return NFCSection::main;
}
NFCSection section_for_field(AuxField) {
    return NFCSection::auxiliary;
}

PrusaNFCReader::NFCTagField field(auto f, NFCTagID tag = 0) {
    return { .tag = tag, .section = section_for_field(f), .field = std::to_underlying(f) };
};

std::ostream &operator<<(std::ostream &os, const PrusaNFCReader::WriteReport &report) {
    os << "{existed: " << report.field_existed << "}";
    return os;
}

template <typename A>
bool std::operator==(const std::span<A> &a, const std::ranges::range auto &b) {
    return std::ranges::equal(a, b);
}

void save_tag_data(const std::string_view &filename, const ByteString &data) {
    std::ofstream f;
    f.open(std::string(filename), std::ios_base::out | std::ios_base::binary);
    f.write((const char *)data.data(), data.size());
}

void test_standard_tag_main(PrusaNFCReader &reader, MockNFCReader &mock) {
    mock.log = {};

    std::array<char, 128> string_buffer;
    std::array<std::byte, 128> bytes_buffer;
    std::array<uint16_t, 2> tags_buffer;
    std::array<NFCField, 32> fields_buffer;

    const std::array expected_fields {
        (NFCField)MainField::material_class,
        (NFCField)MainField::material_type,
        (NFCField)MainField::brand,
        (NFCField)MainField::material_name,
        (NFCField)MainField::is_signed,
        (NFCField)MainField::netto_full_weight,
        (NFCField)MainField::transmission_distance,
        (NFCField)MainField::tags,
        (NFCField)MainField::brand_specific_instance_id,
        (NFCField)MainField::material_uuid,
    };
    CHECK(reader.enumerate_fields(0, NFCRegion::main, fields_buffer) == expected_fields);

    CHECK(reader.read_field_bool(field(MainField::is_signed)) == false);

    CHECK(reader.read_field_int32(field(MainField::material_type)) == std::to_underlying(MaterialType::ASA));
    CHECK(reader.read_field_int32(field(MainField::material_class)) == std::to_underlying(MaterialClass::FFF));

    CHECK(reader.read_field_int64(field(MainField::netto_full_weight)) == 0x1ffffffff);
    CHECK(reader.read_field_float(field(MainField::material_type)) == std::to_underlying(MaterialType::ASA));

    CHECK_THAT(reader.read_field_float(field(MainField::transmission_distance)).value_or(0), Catch::Matchers::WithinAbs(0.3f, 0.001f));

    CHECK(reader.read_field_string(field(MainField::brand), string_buffer) == std::string_view("Prusament"));
    CHECK(reader.read_field_string(field(MainField::material_name), string_buffer) == std::string_view("PLA Prusa Galaxy Black"));

    CHECK(reader.read_field_bytes(field(MainField::brand_specific_instance_id), bytes_buffer) == ByteString { std::byte { 0x01 } });
    CHECK(reader.read_field_bytes(field(MainField::material_uuid), bytes_buffer) == tag_data::material_uuid);

    CHECK(reader.read_field_uint16_array(field(MainField::tags), tags_buffer) == std::array { (uint16_t)MaterialTag::glitter, (uint16_t)MaterialTag::abrasive });

    // Problematic reads
    std::array<char, 4> small_string_buffer;
    std::array<uint16_t, 1> small_tags_buffer;

    CHECK(reader.read_field_int32(field(MainField::netto_full_weight)) == std::unexpected(PrusaNFCReader::Error::data_too_big));

    CHECK(reader.read_field_int32(field(MainField::transmission_distance)) == std::unexpected(PrusaNFCReader::Error::wrong_field_type));

    CHECK(reader.read_field_string(field(MainField::material_name), small_string_buffer) == std::unexpected(PrusaNFCReader::Error::data_too_big));
    CHECK(reader.read_field_int32(field(MainField::is_signed)) == std::unexpected(PrusaNFCReader::Error::wrong_field_type));
    CHECK(reader.read_field_int32(field(MainField::color_ral)) == std::unexpected(PrusaNFCReader::Error::field_not_present));

    CHECK(reader.read_field_uint16_array(field(MainField::tags), small_tags_buffer) == std::unexpected(PrusaNFCReader::Error::data_too_big));

    // Expected one read to cache in the main region
    CHECK(mock.log.reads.size() == 1);
}

TEST_CASE("PrusaNFCReader::reading_standard_sample_tag") {
    MockNFCReader mock;
    PrusaNFCReader reader(mock);

    mock.tag_data[0] = tag_data::standard_sample_tag;

    // Verify metadata structure
    {
        auto meta_r = reader.read_metadata(0);

        // Expecting three reads - NDEF header, NDEF type, metadata region
        CHECK(mock.log.reads.size() == 3);
        CHECK(mock.log.writes.size() == 0);

        REQUIRE(meta_r.has_value());
        REQUIRE(*meta_r);

        const PrusaNFCReader::TagMetadata &meta = **meta_r;
        CHECK(meta.is_valid);

        CHECK(meta.meta_region().is_present());
        CHECK(meta.meta_region().span.offset == payload_offset + 0);
        CHECK(meta.meta_region().span.size == 11);

        CHECK(meta.main_region().is_present());
        CHECK(meta.main_region().span.offset == payload_offset + 11);
        CHECK(meta.main_region().span.size == 252);

        CHECK(!meta.region[NFCRegion::auxiliary].is_present());
    }

    // Read from meta region
    {
        mock.log = {};

        CHECK(reader.read_field_int32(field(NFCMetaField::main_region_offset)) == 11);
        CHECK(reader.read_field_int32(field(NFCMetaField::main_region_size)) == std::unexpected(PrusaNFCReader::Error::field_not_present));

        // The meta region should be in the cache from the read_metadata call, no extra reads should have happened
        CHECK(mock.log.reads.size() == 0);
    }

    test_standard_tag_main(reader, mock);

    // Read from aux region
    {
        mock.log = {};

        CHECK(reader.read_field_int32(field(AuxField::consumed_material)) == std::unexpected(PrusaNFCReader::Error::field_not_present));

        // No extra reads - the reader should remmeber that there is no aux section
        CHECK(mock.log.reads.size() == 0);
    }
}

TEST_CASE("PrusaNFCReader::reading_standard_sample_tag_with_aux") {
    MockNFCReader mock;
    PrusaNFCReader reader(mock);

    mock.tag_data[0] = tag_data::standard_sample_tag_with_aux;

    std::array<NFCField, 32> fields_buffer;

    // Verify metadata structure
    {
        auto meta_r = reader.read_metadata(0);

        // Expecting three reads - NDEF header, NDEF type, metadata region
        CHECK(mock.log.reads.size() == 3);
        CHECK(mock.log.writes.size() == 0);

        // Reset log, start tracking from new
        mock.log = {};

        REQUIRE(meta_r.has_value());
        REQUIRE(*meta_r);

        const PrusaNFCReader::TagMetadata &meta = **meta_r;
        CHECK(meta.is_valid);

        const auto &meta_region = meta.region[NFCRegion::meta];
        CHECK(meta_region.is_present());
        CHECK(meta_region.span.offset == payload_offset + 0);
        CHECK(meta_region.span.size == 11);

        const auto &main_region = meta.region[NFCRegion::main];
        CHECK(main_region.is_present());
        CHECK(main_region.span.offset == payload_offset + 11);
        CHECK(main_region.span.size == 220);

        const auto &aux_region = meta.region[NFCRegion::auxiliary];
        CHECK(aux_region.is_present());
        CHECK(aux_region.span.offset == payload_offset + 231);
        CHECK(aux_region.span.size == 32);
    }

    // Read from meta region
    {
        mock.log = {};

        CHECK(reader.read_field_int32(field(NFCMetaField::main_region_offset)) == 11);
        CHECK(reader.read_field_int32(field(NFCMetaField::aux_region_offset)) == 231);

        // The meta region should be in the cache from the read_metadata call, no extra reads should have happened
        CHECK(mock.log.reads.size() == 0);
    }

    test_standard_tag_main(reader, mock);

    // Read from aux region
    {
        mock.log = {};

        CHECK(reader.enumerate_fields(0, NFCRegion::auxiliary, fields_buffer) == std::array { (NFCField)AuxField::consumed_material });
        CHECK(reader.read_field_int32(field(AuxField::consumed_material)) == 100);

        // Expected one read reading the aux section
        CHECK(mock.log.reads.size() == 1);
    }
}

TEST_CASE("PrusaNFCReader::writing") {
    MockNFCReader mock;
    PrusaNFCReader reader(mock);

    std::array<char, 128> string_buffer;
    std::array<NFCField, 32> fields_buffer;

    mock.tag_data[0] = tag_data::standard_sample_tag_with_aux;

    CHECK(reader.read_field_int32(field(MainField::material_type)) == std::to_underlying(MaterialType::ASA));
    // Expect reads of NDEF header, mime type, meta region, main region
    CHECK(mock.log.reads.size() == 4);
    mock.log = {};

    CHECK(reader.read_field_int32(field(AuxField::consumed_material)) == 100);
    // Expect read of aux region
    CHECK(mock.log.reads.size() == 1);
    mock.log = {};

    // Try writing an int while we have a different region cached
    {
        CHECK(reader.write_field_int32(field(MainField::material_type), std::to_underlying(MaterialType::PETG)) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });
        // We had aux region cached last, so main region read is expected
        CHECK(mock.log.reads.size() == 1);
        // And one write
        REQUIRE(mock.log.writes.size() == 1);
        CHECK(mock.log.writes[0].bytes == 1);
        mock.log = {};

        CHECK(reader.read_field_int32(field(MainField::material_type)) == std::to_underlying(MaterialType::PETG));
        CHECK(mock.tag_data[0] == tag_data::write_sample_1);
        // save_tag_data("write_sample_1.bin", mock.tag_data[0]);

        // The write op should have updated the read cache, so we expect no reads
        CHECK(mock.log.reads.size() == 0);
    }

    // Try writing a string
    {
        CHECK(reader.read_field_string(field(MainField::material_name), string_buffer) == std::string_view("PLA Prusa Galaxy Black"));
        CHECK(reader.write_field_string(field(MainField::material_name), "PLA Kura Balaxy Klack") == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });

        CHECK(mock.log.writes.size() == 1);
        CHECK(mock.log.reads.size() == 0);
        mock.log = {};

        // Try both reads again
        CHECK(reader.read_field_int32(field(MainField::material_type)) == std::to_underlying(MaterialType::PETG));
        CHECK(reader.read_field_string(field(MainField::material_name), string_buffer) == std::string_view("PLA Kura Balaxy Klack"));
        CHECK(mock.tag_data[0] == tag_data::write_sample_2);
        // save_tag_data("write_sample_2.bin", mock.tag_data[0]);

        CHECK(mock.log.reads.size() == 0);
        mock.log = {};
    }

    // Try writing a field that already wasn't there
    {
        CHECK(reader.read_field_float(field(MainField::min_nozzle_diameter)) == std::unexpected(PrusaNFCReader::Error::field_not_present));
        CHECK(reader.write_field_float(field(MainField::min_nozzle_diameter), 12.5f) == PrusaNFCReader::WriteReport { .field_existed = false, .changed = true });
        CHECK(mock.tag_data[0] == tag_data::write_sample_3);
        // save_tag_data("write_sample_3.bin", mock.tag_data[0]);

        CHECK(reader.read_field_float(field(MainField::min_nozzle_diameter)) == 12.5f);

        CHECK(mock.log.writes.size() == 1);
        CHECK(mock.log.reads.size() == 0);
        CHECK(mock.log.writes[0].bytes == 5);
        mock.log = {};
    }

    // Try writing into the aux region
    {
        CHECK(reader.write_field_float(field(AuxField::consumed_material), 969.12f) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });
        CHECK(mock.tag_data[0] == tag_data::write_sample_4);
        // save_tag_data("write_sample_4.bin", mock.tag_data[0]);

        CHECK_THAT(reader.read_field_float(field(AuxField::consumed_material)).value_or(0), Catch::Matchers::WithinAbs(969.12f, 0.001f));

        CHECK(mock.log.writes.size() == 1);
        CHECK(mock.log.reads.size() == 1);
        CHECK(mock.log.writes[0].bytes == 6);
        mock.log = {};
    }

    // Try writing float that can be encoded as int
    {
        CHECK(reader.write_field_float(field(AuxField::consumed_material), 12) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });
        CHECK(mock.tag_data[0] == tag_data::write_sample_5);
        // save_tag_data("write_sample_5.bin", mock.tag_data[0]);

        CHECK(reader.read_field_float(field(AuxField::consumed_material)) == 12);
        CHECK(reader.read_field_int32(field(AuxField::consumed_material)) == 12);

        CHECK(mock.log.writes.size() == 1);
        CHECK(mock.log.reads.size() == 0);
        mock.log = {};
    }

    // Try writing the same value again
    {
        CHECK(reader.write_field_float(field(AuxField::consumed_material), 12) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = false });

        CHECK(mock.log.writes.size() == 0);
        CHECK(mock.log.reads.size() == 0);
        mock.log = {};
    }

    // Try writing the same value in a non-cached region
    {
        CHECK(reader.write_field_float(field(MainField::material_type), std::to_underlying(MaterialType::PETG)) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = false });

        CHECK(mock.log.writes.size() == 0);
        CHECK(mock.log.reads.size() == 1);
        mock.log = {};
    }

    // Remove a field from the main region
    {
        CHECK(reader.remove_field(field(MainField::material_type)) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });

        CHECK(mock.log.writes.size() == 1);
        CHECK(mock.log.reads.size() == 0);
        CHECK(mock.tag_data[0] == tag_data::write_sample_6);
        // save_tag_data("write_sample_6.bin", mock.tag_data[0]);
        mock.log = {};

        CHECK(reader.read_field_float(field(MainField::material_type)) == std::unexpected(PrusaNFCReader::Error::field_not_present));
        CHECK(reader.remove_field(field(MainField::material_type)) == PrusaNFCReader::WriteReport { .field_existed = false, .changed = false });
        CHECK(mock.log.reads.size() == 0);
    }

    // Remove a field from the aux region
    {
        CHECK(reader.remove_field(field(AuxField::consumed_material)) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });

        CHECK(mock.log.writes.size() == 1);
        CHECK(mock.log.reads.size() == 1);
        CHECK(mock.tag_data[0] == tag_data::write_sample_7);
        // save_tag_data("write_sample_7.bin", mock.tag_data[0]);
        mock.log = {};

        CHECK(reader.read_field_float(field(AuxField::consumed_material)) == std::unexpected(PrusaNFCReader::Error::field_not_present));
        CHECK(reader.remove_field(field(AuxField::consumed_material)) == PrusaNFCReader::WriteReport { .field_existed = false, .changed = false });
        CHECK(reader.enumerate_fields(0, NFCRegion::auxiliary, fields_buffer) == std::span<NFCField> {});

        CHECK(mock.log.reads.size() == 0);
    }

    // Try writing bytes
    {
        CHECK(reader.write_field_bytes(field(MainField::brand_specific_instance_id), ByteString { std::byte(1), std::byte(2) }) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });
        CHECK(mock.tag_data[0] == tag_data::write_sample_8);
        mock.log = {};
    }

    // Try writing tags
    {
        std::array<uint16_t, 8> tags_buffer;
        auto r = reader.read_field_uint16_array(field(MainField::tags), tags_buffer);
        REQUIRE(r.has_value());

        size_t cnt = r->size();
        REQUIRE(cnt < tags_buffer.size());

        tags_buffer[cnt++] = (uint16_t)MaterialTag::flexible;

        CHECK(reader.write_field_uint16_array(field(MainField::material_name), std::span(tags_buffer.data(), cnt)) == PrusaNFCReader::WriteReport { .field_existed = true, .changed = true });

        CHECK(mock.log.writes.size() == 1);
        mock.log = {};
    }
}

TEST_CASE("PrusaNFCReader::edge_cases") {
    MockNFCReader mock;
    PrusaNFCReader reader(mock);

    SECTION("Try writing a string that definitely doesn't fit into the memory") {
        mock.tag_data[0] = tag_data::standard_sample_tag_with_aux;

        std::vector<char> str;
        str.resize(300);
        std::ranges::fill(str, 'a');

        CHECK(reader.write_field_string(field(MainField::material_name), std::string_view(str.data(), str.size())) == std::unexpected(PrusaNFCReader::Error::data_too_big));

        str.resize(200);
        CHECK(reader.write_field_string(field(MainField::material_name), std::string_view(str.data(), str.size())) == std::unexpected(PrusaNFCReader::Error::data_too_big));

        CHECK(mock.log.writes.size() == 0);
    }

    SECTION("Enumerate fields with a small buffer") {
        mock.tag_data[0] = tag_data::standard_sample_tag_with_aux;

        std::array<NFCField, 7> fields_buffer;
        CHECK(reader.enumerate_fields(0, NFCRegion::main, fields_buffer) == std::unexpected(PrusaNFCReader::Error::data_too_big));
    }

    SECTION("Tags with invalid metadata") {
        SECTION("Tag with main section outside of the tag itself") {
            mock.tag_data[0] = tag_data::corrupt_tag_1;
        }

        SECTION("Tag with main section bigger than the payload") {
            mock.tag_data[0] = tag_data::corrupt_tag_2;
        }

        CHECK(reader.read_field_int32(field(MainField::brand)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));

        // Expecting reads of ndef header, ID and meta region
        CHECK(mock.log.reads.size() == 3);
        mock.log = {};

        // The tag should be marked as invalid in the cache and we shouldn't be getting any reads
        CHECK(reader.read_field_bool(field(AuxField::consumed_material)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
        CHECK(reader.write_field_int32(field(MainField::brand), 0) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
        CHECK(mock.log.reads.size() == 0);
        CHECK(mock.log.writes.size() == 0);
    }

    SECTION("Tags with a different mime type") {
        mock.tag_data[0] = tag_data::wrong_mime;
        mock.tag_data[1] = tag_data::wrong_mime_2;
        mock.tag_data[2] = tag_data::standard_sample_tag;

        CHECK(reader.read_field_int32(field(MainField::brand, 0)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
        UNSCOPED_INFO("Expecting reads of ndef header. Different MIME type length, so we don't even don't need to read it");
        CHECK(mock.log.reads.size() == 1);
        mock.log = {};

        CHECK(reader.read_field_float(field(MainField::brand, 1)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
        UNSCOPED_INFO("Expecting reads of ndef header + mime ID");
        CHECK(mock.log.reads.size() == 2);
        mock.log = {};

        CHECK(reader.read_field_int32(field(MainField::material_type, 2)) == std::to_underlying(MaterialType::ASA));
        UNSCOPED_INFO("Read ndef header, mime type, metadata, main data");
        CHECK(mock.log.reads.size() == 4);
        mock.log = {};

        {
            INFO("Information that the tags are invalid should be stored in the cache");
            CHECK(reader.read_field_int32(field(MainField::material_type, 0)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
            CHECK(mock.log.reads.size() == 0);

            CHECK(reader.read_field_int32(field(MainField::material_type, 1)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
            CHECK(mock.log.reads.size() == 0);
            mock.log = {};
        }

        {
            INFO("Invalidating tag 1 should result in the tag being re-read when requested");
            reader.invalidate_cache(1);

            CHECK(reader.read_field_int32(field(MainField::material_type, 0)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
            CHECK(mock.log.reads.size() == 0);

            CHECK(reader.read_field_int32(field(MainField::material_type, 1)) == std::unexpected(PrusaNFCReader::Error::tag_invalid));
            UNSCOPED_INFO("Should read header & mime type");
            CHECK(mock.log.reads.size() == 2);
            mock.log = {};
        }
    }

    SECTION("Tag with a malformed section") {
        std::array<char, 128> string_buffer;

        auto tag_data = tag_data::standard_sample_tag;
        std::fill(tag_data.begin() + 64, tag_data.end(), std::byte('\xbf')); // bf - indefinite container open
        mock.tag_data[0] = tag_data;

        {
            INFO("Reads in the non-damaged part - should return values still");
            CHECK(reader.read_field_int32(field(NFCMetaField::main_region_offset)) == 11);
            CHECK(mock.log.reads.size() == 3);
            mock.log = {};

            CHECK(reader.read_field_string(field(MainField::brand), string_buffer) == std::string_view("Prusament"));
            CHECK(mock.log.reads.size() == 1);
            mock.log = {};
        }

        {
            INFO("Reads in the damaged part - should return error");
            CHECK(reader.read_field_int32(field(MainField::transmission_distance)) == std::unexpected(PrusaNFCReader::Error::region_corrupt));
        }

        {
            INFO("Writing to the damaged part should fail without any writes")
            CHECK(reader.write_field_float(field(MainField::transmission_distance), 18) == std::unexpected(PrusaNFCReader::Error::region_corrupt));
            CHECK(mock.log.reads.size() == 0); // Should have been cached
            CHECK(mock.log.writes.size() == 0);
            mock.log = {};
        }
    }

    SECTION("Tag without the container open") {
        auto tag_data = tag_data::standard_sample_tag;
        mock.tag_data[0] = tag_data;

        const auto main_region_span = (**reader.read_metadata(0)).main_region().span;

        // Zero out the whole main region - that means that there is no cbor container tag at the beginning
        std::fill(tag_data.begin() + main_region_span.offset, tag_data.begin() + main_region_span.end(), std::byte(0));

        mock.tag_data[0] = tag_data;
        reader.invalidate_cache(0);

        // Meta region should be okay
        CHECK(reader.read_field_int32(field(NFCMetaField::main_region_offset)) == 11);

        // Main region should return corrupt
        CHECK(reader.read_field_int32(field(MainField::brand)) == std::unexpected(PrusaNFCReader::Error::region_corrupt));

        // Meta region should be still okay
        CHECK(reader.read_field_int32(field(NFCMetaField::main_region_offset)) == 11);

        // Writing to the main region should fail
        CHECK(reader.write_field_float(field(MainField::transmission_distance), 18) == std::unexpected(PrusaNFCReader::Error::region_corrupt));
    }

    SECTION("Minimal meta section") {
        mock.tag_data[0] = tag_data::empty_tag_with_minimal_meta_section;

        // This tag is smaller, so the NDEF payload length is encoded into 1 B instead of 4
        constexpr NFCOffset payload_offset = 34;

        auto meta_r = reader.read_metadata(0);
        REQUIRE(meta_r.has_value());

        const auto &meta = **meta_r;
        CHECK(meta.is_valid);

        CHECK(meta.meta_region().is_present());
        CHECK(meta.meta_region().span.offset == payload_offset + 0);
        CHECK(meta.meta_region().span.size == 1);

        CHECK(meta.main_region().is_present());
        CHECK(meta.main_region().span.offset == payload_offset + 1);
        CHECK(meta.main_region().span.size == 93);

        CHECK(!meta.aux_region().is_present());
    }

    SECTION("Big meta section") {
        mock.tag_data[0] = tag_data::empty_tag_with_big_meta_section;

        // This tag is smaller, so the NDEF payload length is encoded into 1 B instead of 4
        constexpr NFCOffset payload_offset = 34;

        auto meta_r = reader.read_metadata(0);
        REQUIRE(meta_r.has_value());

        const auto &meta = **meta_r;
        CHECK(meta.is_valid);

        CHECK(meta.meta_region().is_present());
        CHECK(meta.meta_region().span.offset == payload_offset + 0);
        CHECK(meta.meta_region().span.size == 32);

        CHECK(meta.main_region().is_present());
        CHECK(meta.main_region().span.offset == payload_offset + 32);
        CHECK(meta.main_region().span.size == 174);

        CHECK(meta.aux_region().is_present());
        CHECK(meta.aux_region().span.offset == payload_offset + 206);
        CHECK(meta.aux_region().span.size == 16);
    }

    SECTION("Disallow writing to the meta section") {
        mock.tag_data[0] = tag_data::standard_sample_tag_with_aux;
        CHECK(reader.write_field_int32(field(NFCMetaField::aux_region_offset), 0) == std::unexpected(PrusaNFCReader::Error::write_protected));
    }

    SECTION("Disallow writing to the meta section") {
        mock.tag_data[0] = tag_data::standard_sample_tag_with_aux;
        CHECK(reader.write_field_int32(field(NFCMetaField::aux_region_offset), 0) == std::unexpected(PrusaNFCReader::Error::write_protected));
    }
}

TEST_CASE("PrusaNFCReader::caching") {
    MockNFCReader mock;
    PrusaNFCReader reader(mock);
    for (size_t i = 0; i < PrusaNFCReader::metadata_cache_capacity + 2; i++) {
        mock.tag_data[(NFCTagID)i] = tag_data::standard_sample_tag_with_aux;
    }

    const auto check_full_read = [&](NFCTagID tag) {
        INFO("Full read " << tag);

        auto meta_r = reader.read_metadata(tag);
        CHECK(meta_r.has_value());

        // Expecting reads of ndef header, ID and meta region
        CHECK(mock.log.reads.size() == 3);
        for (const auto &read : mock.log.reads) {
            CHECK(read.tag == tag);
        }
        mock.log = {};
    };

    const auto check_no_read = [&](NFCTagID tag) {
        INFO("No read " << tag);

        auto meta_r = reader.read_metadata(tag);
        CHECK(meta_r.has_value());

        // Expecting reads of ndef header, ID and meta region
        CHECK(mock.log.reads.size() == 0);
        mock.log = {};
    };

    for (size_t i = 0; i < PrusaNFCReader::metadata_cache_capacity; i++) {
        INFO("Fill the cache");
        check_full_read(i);
    }

    for (size_t i = 0; i < PrusaNFCReader::metadata_cache_capacity; i++) {
        INFO("Read again through all items that should be in the cache - no reads should be needed for reading metadata");
        check_no_read(i);
    }

    {
        INFO("Now read another chip, exceeding the capacity");
        check_full_read(PrusaNFCReader::metadata_cache_capacity);
    }

    {
        INFO("Reading again the same should result in no reads");
        check_no_read(PrusaNFCReader::metadata_cache_capacity);
    }

    {
        INFO("But the oldest-accessed tag should have been thrown out of the cache");
        INFO("Now we should have tag 0 replacing tag 1")
        check_full_read(0);
        check_no_read(0);
    }

    {
        INFO("Tag 2 should be in the cache still");
        check_no_read(2);
        check_no_read(0);
        check_no_read(PrusaNFCReader::metadata_cache_capacity);
    }

    {
        INFO("Tag 1 should be out of the cache now. Reading it should replace tag 2");
        check_full_read(1);
        check_full_read(3);
    }
}

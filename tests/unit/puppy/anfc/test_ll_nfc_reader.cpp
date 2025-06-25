#include <prusa_nfc_nfcv/ll_nfc_reader.hpp>

#include <nfcv/rw_interface.hpp>
#include <catch2/catch.hpp>

#include <algorithm>
#include <vector>
#include <variant>
#include <deque>
#include <map>

struct FieldUp {
    nfcv::ReaderWriterInterface::AntennaData antenna;

    bool operator==(const FieldUp &other) const {
        return antenna == other.antenna;
    }
};

struct FieldDown {
    bool operator==(const FieldDown &) const {
        return true;
    }
};

struct SwitchToNextDiscoveryAntenna {
    bool operator==(const SwitchToNextDiscoveryAntenna &) const {
        return true;
    }
};

struct Inventory {
    bool operator==(const Inventory &) const {
        return true;
    }
};

struct StayQuiet {
    nfcv::UID uid;
    bool operator==(const StayQuiet &other) const {
        return uid == other.uid;
    }
};

struct SystemInfo {
    nfcv::UID uid;
    bool operator==(const SystemInfo &other) const {
        return uid == other.uid;
    }
};

struct ReadSingleBlock {
    nfcv::UID uid;
    nfcv::BlockID block_id;
    bool operator==(const ReadSingleBlock &other) const {
        return uid == other.uid && block_id == other.block_id;
    }
};

struct WriteSingleBlock {
    nfcv::UID uid;
    nfcv::BlockID block_id;
    std::vector<std::byte> data;
    bool operator==(const WriteSingleBlock &other) const {
        return uid == other.uid && block_id == other.block_id && data == other.data;
    }
};

using Event = std::variant<FieldUp, FieldDown, SwitchToNextDiscoveryAntenna, Inventory, StayQuiet, SystemInfo, ReadSingleBlock, WriteSingleBlock>;

struct EventLogger : public nfcv::ReaderWriterInterface {
    nfcv::Result<void> field_up(AntennaData antenna_data) final {
        events.push_back(FieldUp { .antenna = antenna_data });
        return {};
    }
    void field_down() final {
        events.push_back(FieldDown {});
    }

    AntennaData switch_to_next_discovery_atenna() final {
        ++antenna_index;
        if (antenna_index >= fake_antennas.size()) {
            antenna_index %= fake_antennas.size();
        }

        events.push_back(SwitchToNextDiscoveryAntenna {});
        return static_cast<AntennaData>(antenna_index);
    }

    [[nodiscard]] nfcv::Result<void> nfcv_command(nfcv::Command &command) final {
        return std::visit([&](auto &command) { return nfcv_command_impl(command); }, command);
    }

    nfcv::Result<void> nfcv_command_impl(nfcv::command::Inventory &command) {
        events.push_back(Inventory {});
        auto &curr_queue = fake_antennas[antenna_index];
        if (curr_queue.size() > 0) {
            const auto element = curr_queue.front();
            curr_queue.pop_front();
            if (element.has_value()) {
                std::ranges::copy(*element, command.response.begin());
                return {};
            }
        }

        return std::unexpected(nfcv::Error::no_response);
    }

    nfcv::Result<void> nfcv_command_impl(nfcv::command::StayQuiet &command) {
        StayQuiet stay_quiet {};
        std::copy_n(command.request.uid.begin(), command.request.uid.size(), stay_quiet.uid.begin());
        events.push_back(stay_quiet);
        return {};
    }

    nfcv::Result<void> nfcv_command_impl(nfcv::command::SystemInfo &command) {
        SystemInfo sys_info {};
        std::copy_n(command.request.uid.begin(), command.request.uid.size(), sys_info.uid.begin());
        events.push_back(sys_info);

        auto tag_data = std::ranges::find_if(tag_infos, [&](const auto &element) { return std::ranges::equal(command.request.uid, element.first); });
        if (tag_data != tag_infos.end()) {
            command.response = tag_data->second;
            return {};
        }

        return std::unexpected(nfcv::Error::other);
    }

    nfcv::Result<void> nfcv_command_impl(nfcv::command::ReadSingleBlock &command) {
        const auto &buffer = command.response;

        ReadSingleBlock read_block { .uid = {}, .block_id = command.request.block_address };
        std::ranges::copy(command.request.uid, read_block.uid.begin());
        events.push_back(read_block);

        auto tag = std::ranges::find_if(tags, [&](const auto &element) { return std::ranges::equal(command.request.uid, element.first); });
        if (tag != tags.end()) {
            auto it = tag->second.get().begin();
            std::advance(it, command.request.block_address * buffer.size());
            std::copy_n(it, buffer.size(), buffer.begin());
        }
        return {};
    }

    nfcv::Result<void> nfcv_command_impl(nfcv::command::WriteSingleBlock &command) {
        const auto &buffer = command.request.block_buffer;

        WriteSingleBlock write_block { .uid = {}, .block_id = command.request.block_address, .data = {} };
        std::ranges::copy(command.request.uid, write_block.uid.begin());
        std::ranges::copy(buffer, std::back_insert_iterator { write_block.data });
        events.push_back(write_block);

        auto tag = std::ranges::find_if(tags, [&](const auto &element) { return std::ranges::equal(command.request.uid, element.first); });
        if (tag != tags.end()) {
            auto it = tag->second.get().begin();
            std::advance(it, command.request.block_address * buffer.size());
            std::copy_n(buffer.begin(), buffer.size(), it);
        }
        return {};
    }

    std::vector<Event> events {};
    /// Fakes antennas and emulates inventory calls
    /// First layer is vector of antennas
    /// Second layer is deque of possible tags discovareble by calling inventory command
    /// Inventory will accept UID until std::nullopt. Multiple std::nullopts after each other are valid.
    std::vector<std::deque<std::optional<nfcv::UID>>> fake_antennas {};
    std::map<nfcv::UID, nfcv::TagInfo> tag_infos {};
    std::map<nfcv::UID, std::reference_wrapper<std::vector<std::byte>>> tags {};
    size_t antenna_index {};
};

namespace data {
constexpr nfcv::UID uid1 { std::byte { 0x01 }, std::byte { 0x02 }, std::byte { 0x02 }, std::byte { 0x03 }, std::byte { 0x39 }, std::byte { 0x01 }, std::byte { 0x04 }, nfcv::UID_MSB };
constexpr nfcv::UID uid2 { std::byte { 0x04 }, std::byte { 0x05 }, std::byte { 0x06 }, std::byte { 0x07 }, std::byte { 0x39 }, std::byte { 0x01 }, std::byte { 0x04 }, nfcv::UID_MSB };
constexpr nfcv::UID uid3 { std::byte { 0x08 }, std::byte { 0x09 }, std::byte { 0x0a }, std::byte { 0x0b }, std::byte { 0x39 }, std::byte { 0x01 }, std::byte { 0x04 }, nfcv::UID_MSB };
constexpr nfcv::TagInfo tag_info1 { .dsfid = std::nullopt, .afi = std::nullopt, .mem_size = nfcv::TagInfo::MemorySize { .block_size = 4, .block_count = 64 }, .ic_ref = std::nullopt };
} // namespace data

TEST_CASE("Test NFC-V tag discovery and tag lost detection", "[nfcv][prusa_nfc]") {
    EventLogger logger {};

    LLNFCReader reader(logger);
    INFCReader::Event event;

    SECTION("Test simple TagDisovered -> TagLost functionality") {
        logger.events = {};
        logger.fake_antennas = { { data::uid1, std::nullopt, std::nullopt }, {} };
        logger.tag_infos = { std::pair { data::uid1, data::tag_info1 } };
        logger.tags = {};
        logger.antenna_index = 1;

        auto res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 0);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid1 },
                                                    SystemInfo { .uid = data::uid1 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
        logger.events.clear();

        res = reader.get_event(event);
        REQUIRE(res == false);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 1 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
        logger.events.clear();

        res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagLostEvent>(event));
        REQUIRE(std::get<INFCReader::TagLostEvent>(event).tag == 0);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
    }

    SECTION("Test mutliple tags discovered scenario - all at once") {
        logger.events = {};
        logger.fake_antennas = { { data::uid1, data::uid2 }, {} };
        logger.tag_infos = { std::pair { data::uid1, data::tag_info1 }, std::pair { data::uid2, data::tag_info1 } };
        logger.tags = {};
        logger.antenna_index = 1;

        auto res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 0);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid1 },
                                                    SystemInfo { .uid = data::uid1 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid2 },
                                                    SystemInfo { .uid = data::uid2 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 1);
        CHECK(logger.events.empty());

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == false);
    }

    SECTION("Test mutliple tags discovered scenario - on multiple antennas") {
        logger.events = {};
        logger.fake_antennas = { { data::uid1, data::uid2 }, { data::uid3 } };
        logger.tag_infos = { std::pair { data::uid1, data::tag_info1 }, std::pair { data::uid2, data::tag_info1 }, std::pair { data::uid3, data::tag_info1 } };
        logger.tags = {};
        logger.antenna_index = 1;

        auto res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 0);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid1 },
                                                    SystemInfo { .uid = data::uid1 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid2 },
                                                    SystemInfo { .uid = data::uid2 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 1);
        CHECK(logger.events.empty());

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 2);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 1 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid3 },
                                                    SystemInfo { .uid = data::uid3 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
    }

    SECTION("Test tag switch antenna scenario") {
        logger.events = {};
        logger.fake_antennas = { { data::uid1, data::uid2, std::nullopt, data::uid1, std::nullopt, data::uid1, std::nullopt, data::uid1 }, { std::nullopt, data::uid2, std::nullopt, data::uid2, std::nullopt, data::uid2 } };
        logger.tag_infos = { std::pair { data::uid1, data::tag_info1 }, std::pair { data::uid2, data::tag_info1 } };
        logger.tags = {};
        logger.antenna_index = 1;

        auto res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 0);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid1 },
                                                    SystemInfo { .uid = data::uid1 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid2 },
                                                    SystemInfo { .uid = data::uid2 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 1);
        CHECK(logger.events.empty());

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == false);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 1 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagLostEvent>(event));
        CHECK(std::get<INFCReader::TagLostEvent>(event).tag == 1);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid1 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == false);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 1 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid2 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == false);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid1 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == false);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 1 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid2 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));

        reader.forget_tag(1);

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == false);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 0 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid1 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));

        logger.events.clear();
        res = reader.get_event(event);
        REQUIRE(res == true);
        REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
        CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 1);
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    SwitchToNextDiscoveryAntenna {},
                                                    FieldUp { .antenna = 1 },
                                                    Inventory {},
                                                    StayQuiet { .uid = data::uid2 },
                                                    SystemInfo { .uid = data::uid2 },
                                                    Inventory {},
                                                    FieldDown {},
                                                }));
    }
}

TEST_CASE("Test NFC-V tag read ops", "[nfcv][prusa_nfc]") {
    std::vector<std::byte> tag1 {};
    tag1.resize(data::tag_info1.mem_size.value().block_size * data::tag_info1.mem_size.value().block_count);
    std::ranges::generate(tag1, [n = 0]() mutable { return static_cast<std::byte>(n++); });
    EventLogger logger {};
    logger.events = {};
    logger.fake_antennas = { { data::uid1, std::nullopt } };
    logger.tag_infos = { std::pair { data::uid1, data::tag_info1 } };
    logger.tags = { std::pair { data::uid1, std::reference_wrapper { tag1 } } };
    logger.antenna_index = 0;

    LLNFCReader reader { logger };

    INFCReader::Event event;
    auto res = reader.get_event(event);
    REQUIRE(res == true);
    REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
    CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 0);
    logger.events.clear();

    SECTION("Reading small chunk of data (inside of 1 block)") {
        std::array<std::byte, 2> data;
        data.fill(std::byte { 0 });
        const auto read_res = reader.read(0, 1, std::span { data });

        REQUIRE(read_res.has_value());
        REQUIRE(std::ranges::all_of(std::views::zip(data, std::views::iota(1)), [](const auto &item) { return std::get<0>(item) == static_cast<std::byte>(std::get<1>(item)); }));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 0 },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Reading small chunk of data (on the edge of a block)") {
        std::array<std::byte, 2> data;
        data.fill(std::byte { 0 });
        const auto read_res = reader.read(0, 3, std::span { data });

        REQUIRE(read_res.has_value());
        REQUIRE(std::ranges::all_of(std::views::zip(data, std::views::iota(3)), [](const auto &item) { return std::get<0>(item) == static_cast<std::byte>(std::get<1>(item)); }));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 1 },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Reading multiple blocks of data alligned") {
        std::array<std::byte, 32> data;
        data.fill(std::byte { 0 });
        const auto read_res = reader.read(0, 16, std::span { data });

        REQUIRE(read_res.has_value());
        REQUIRE(std::ranges::all_of(std::views::zip(data, std::views::iota(16)), [](const auto &item) { return std::get<0>(item) == static_cast<std::byte>(std::get<1>(item)); }));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 4 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 5 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 6 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 7 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 8 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 9 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 10 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 11 },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Reading multiple blocks of data unalligned") {
        std::array<std::byte, 36> data;
        data.fill(std::byte { 0 });
        const auto read_res = reader.read(0, 15, std::span { data });

        REQUIRE(read_res.has_value());
        REQUIRE(std::ranges::all_of(std::views::zip(data, std::views::iota(15)), [](const auto &item) { return std::get<0>(item) == static_cast<std::byte>(std::get<1>(item)); }));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 3 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 4 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 5 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 6 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 7 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 8 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 9 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 10 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 11 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 12 },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Reading data on invalid tag") {
        std::array<std::byte, 2> data;
        data.fill(std::byte { 0 });
        const auto read_res = reader.read(1, 3, std::span { data });

        REQUIRE(!read_res.has_value());
        REQUIRE(read_res.error() == INFCReader::IOError::invalid_id);
    }

    SECTION("Reading data out of range - totaly out of range (big offset)") {
        std::array<std::byte, 2> data;
        data.fill(std::byte { 0 });
        const auto read_res = reader.read(0, 270, std::span { data });

        REQUIRE(!read_res.has_value());
        REQUIRE(read_res.error() == INFCReader::IOError::outside_of_bounds);
    }

    SECTION("Reading data out of range - overlaping range") {
        std::array<std::byte, 100> data;
        data.fill(std::byte { 0 });
        const auto read_res = reader.read(0, 250, std::span { data });

        REQUIRE(!read_res.has_value());
        REQUIRE(read_res.error() == INFCReader::IOError::outside_of_bounds);
    }
}

TEST_CASE("Test NFC-V tags write ops", "[nfcv][prusa_nfc]") {
    std::vector<std::byte> tag1 {};
    tag1.resize(data::tag_info1.mem_size.value().block_size * data::tag_info1.mem_size.value().block_count);
    std::ranges::generate(tag1, [n = 0]() mutable { return static_cast<std::byte>(n++); });
    EventLogger logger {};
    logger.events = {};
    logger.fake_antennas = { { data::uid1, std::nullopt } };
    logger.tag_infos = { std::pair { data::uid1, data::tag_info1 } };
    logger.tags = { std::pair { data::uid1, std::reference_wrapper { tag1 } } };
    logger.antenna_index = 0;

    LLNFCReader reader { logger };

    INFCReader::Event event;
    auto res = reader.get_event(event);
    REQUIRE(res == true);
    REQUIRE(std::holds_alternative<INFCReader::TagDetectedEvent>(event));
    CHECK(std::get<INFCReader::TagDetectedEvent>(event).tag == 0);
    logger.events.clear();

    SECTION("Write aligned data") {
        constexpr NFCOffset offset = 0;
        std::array<std::byte, 32> data;
        std::ranges::generate(data, [n = 0]() mutable { return static_cast<std::byte>(255 - (n++)); });
        const auto write_res = reader.write(0, offset, std::span { data });

        REQUIRE(write_res.has_value());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(tag1[i + offset] == data[i]);
        }
        CHECK(tag1[data.size() + offset] == static_cast<std::byte>(data.size() + offset));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 0, .data = std::vector { std::byte { 0xff }, std::byte { 0xfe }, std::byte { 0xfd }, std::byte { 0xfc } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 1, .data = std::vector { std::byte { 0xfb }, std::byte { 0xfa }, std::byte { 0xf9 }, std::byte { 0xf8 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 2, .data = std::vector { std::byte { 0xf7 }, std::byte { 0xf6 }, std::byte { 0xf5 }, std::byte { 0xf4 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 3, .data = std::vector { std::byte { 0xf3 }, std::byte { 0xf2 }, std::byte { 0xf1 }, std::byte { 0xf0 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 4, .data = std::vector { std::byte { 0xef }, std::byte { 0xee }, std::byte { 0xed }, std::byte { 0xec } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 5, .data = std::vector { std::byte { 0xeb }, std::byte { 0xea }, std::byte { 0xe9 }, std::byte { 0xe8 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 6, .data = std::vector { std::byte { 0xe7 }, std::byte { 0xe6 }, std::byte { 0xe5 }, std::byte { 0xe4 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 7, .data = std::vector { std::byte { 0xe3 }, std::byte { 0xe2 }, std::byte { 0xe1 }, std::byte { 0xe0 } } },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Write unaligned data - overflow in the beginning") {
        constexpr NFCOffset offset = 2;
        std::array<std::byte, 18> data;
        std::ranges::generate(data, [n = 0]() mutable { return static_cast<std::byte>(255 - (n++)); });
        const auto write_res = reader.write(0, offset, std::span { data });

        REQUIRE(write_res.has_value());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(tag1[i + offset] == data[i]);
        }
        CHECK(tag1[offset - 1] == static_cast<std::byte>(offset - 1));
        CHECK(tag1[data.size() + offset] == static_cast<std::byte>(data.size() + offset));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 0 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 0, .data = std::vector { std::byte { 0x00 }, std::byte { 0x01 }, std::byte { 0xff }, std::byte { 0xfe } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 1, .data = std::vector { std::byte { 0xfd }, std::byte { 0xfc }, std::byte { 0xfb }, std::byte { 0xfa } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 2, .data = std::vector { std::byte { 0xf9 }, std::byte { 0xf8 }, std::byte { 0xf7 }, std::byte { 0xf6 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 3, .data = std::vector { std::byte { 0xf5 }, std::byte { 0xf4 }, std::byte { 0xf3 }, std::byte { 0xf2 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 4, .data = std::vector { std::byte { 0xf1 }, std::byte { 0xf0 }, std::byte { 0xef }, std::byte { 0xee } } },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Write unaligned data - overflow at the end") {
        constexpr NFCOffset offset = 4;
        std::array<std::byte, 18> data;
        std::ranges::generate(data, [n = 0]() mutable { return static_cast<std::byte>(255 - (n++)); });
        const auto write_res = reader.write(0, offset, std::span { data });

        CAPTURE(data);
        CAPTURE(tag1);
        REQUIRE(write_res.has_value());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(tag1[i + offset] == data[i]);
        }
        CHECK(tag1[offset - 1] == static_cast<std::byte>(offset - 1));
        CHECK(tag1[data.size() + offset] == static_cast<std::byte>(data.size() + offset));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 1, .data = std::vector { std::byte { 0xff }, std::byte { 0xfe }, std::byte { 0xfd }, std::byte { 0xfc } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 2, .data = std::vector { std::byte { 0xfb }, std::byte { 0xfa }, std::byte { 0xf9 }, std::byte { 0xf8 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 3, .data = std::vector { std::byte { 0xf7 }, std::byte { 0xf6 }, std::byte { 0xf5 }, std::byte { 0xf4 } } },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 4, .data = std::vector { std::byte { 0xf3 }, std::byte { 0xf2 }, std::byte { 0xf1 }, std::byte { 0xf0 } } },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 5 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 5, .data = std::vector { std::byte { 0xef }, std::byte { 0xee }, std::byte { 0x16 }, std::byte { 0x17 } } },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Write data smaller then sector - start of sector") {
        constexpr NFCOffset offset = 0;
        std::array<std::byte, 2> data;
        std::ranges::generate(data, [n = 0]() mutable { return static_cast<std::byte>(255 - (n++)); });
        const auto write_res = reader.write(0, offset, std::span { data });

        CAPTURE(data);
        CAPTURE(tag1);
        REQUIRE(write_res.has_value());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(tag1[i + offset] == data[i]);
        }
        CHECK(tag1[data.size() + offset] == static_cast<std::byte>(data.size() + offset));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 0 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 0, .data = std::vector { std::byte { 0xff }, std::byte { 0xfe }, std::byte { 0x02 }, std::byte { 0x03 } } },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Write data smaller then sector - middle of sector") {
        constexpr NFCOffset offset = 1;
        std::array<std::byte, 2> data;
        std::ranges::generate(data, [n = 0]() mutable { return static_cast<std::byte>(255 - (n++)); });
        const auto write_res = reader.write(0, offset, std::span { data });

        CAPTURE(data);
        CAPTURE(tag1);
        REQUIRE(write_res.has_value());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(tag1[i + offset] == data[i]);
        }
        CHECK(tag1[offset - 1] == static_cast<std::byte>(offset - 1));
        CHECK(tag1[data.size() + offset] == static_cast<std::byte>(data.size() + offset));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 0 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 0, .data = std::vector { std::byte { 0x00 }, std::byte { 0xff }, std::byte { 0xfe }, std::byte { 0x03 } } },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Write data smaller then sector - end of sector") {
        constexpr NFCOffset offset = 2;
        std::array<std::byte, 2> data;
        std::ranges::generate(data, [n = 0]() mutable { return static_cast<std::byte>(255 - (n++)); });
        const auto write_res = reader.write(0, offset, std::span { data });

        CAPTURE(data);
        CAPTURE(tag1);
        REQUIRE(write_res.has_value());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(tag1[i + offset] == data[i]);
        }
        CHECK(tag1[offset - 1] == static_cast<std::byte>(offset - 1));
        CHECK(tag1[data.size() + offset] == static_cast<std::byte>(data.size() + offset));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 0 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 0, .data = std::vector { std::byte { 0x00 }, std::byte { 0x01 }, std::byte { 0xff }, std::byte { 0xfe } } },
                                                    FieldDown {},
                                                }));
    }

    SECTION("Write data smaller then sector - overlapping sectors") {
        constexpr NFCOffset offset = 3;
        std::array<std::byte, 2> data;
        std::ranges::generate(data, [n = 0]() mutable { return static_cast<std::byte>(255 - (n++)); });
        const auto write_res = reader.write(0, offset, std::span { data });

        CAPTURE(data);
        CAPTURE(tag1);
        REQUIRE(write_res.has_value());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(tag1[i + offset] == data[i]);
        }
        CHECK(tag1[offset - 1] == static_cast<std::byte>(offset - 1));
        CHECK(tag1[data.size() + offset] == static_cast<std::byte>(data.size() + offset));
        CHECK(std::ranges::equal(logger.events, std::vector<Event> {
                                                    FieldUp { .antenna = 0 },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 0 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 0, .data = std::vector { std::byte { 0x00 }, std::byte { 0x01 }, std::byte { 0x02 }, std::byte { 0xff } } },
                                                    ReadSingleBlock { .uid = data::uid1, .block_id = 1 },
                                                    WriteSingleBlock { .uid = data::uid1, .block_id = 1, .data = std::vector { std::byte { 0xfe }, std::byte { 0x05 }, std::byte { 0x06 }, std::byte { 0x07 } } },
                                                    FieldDown {},
                                                }));
    }
}

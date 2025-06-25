#include <nfcv/rw_interface.hpp>

using namespace nfcv;

Result<UID> ReaderWriterInterface::inventory() {
    command::Inventory::Response response;
    Command cmd { command::Inventory {
        .request {},
        .response { response },
    } };
    const auto res = nfcv_command(cmd);
    if (!res.has_value()) {
        return std::unexpected(res.error());
    }
    return { response };
}

Result<void> ReaderWriterInterface::stay_quiet(const UID &uid) {
    nfcv::Command cmd { nfcv::command::StayQuiet {
        .request { .uid = uid },
    } };
    return nfcv_command(cmd);
}

Result<TagInfo> ReaderWriterInterface::get_system_info(const UID &uid) {
    command::SystemInfo::Response response;
    nfcv::Command cmd { nfcv::command::SystemInfo {
        .request { .uid = uid },
        .response { response },
    } };
    const auto res = nfcv_command(cmd);
    if (!res.has_value()) {
        return std::unexpected(res.error());
    }
    return { response };
}

Result<void> ReaderWriterInterface::read_single_block(const UID &uid, BlockID block_id, const std::span<std::byte> &buffer) {
    nfcv::Command cmd { nfcv::command::ReadSingleBlock {
        .request { .uid = uid, .block_address = block_id },
        .response { buffer },
    } };
    return nfcv_command(cmd);
}

Result<void> ReaderWriterInterface::write_single_block(const UID &uid, BlockID block_id, const std::span<const std::byte> &buffer) {
    nfcv::Command cmd { nfcv::command::WriteSingleBlock {
        .request { .uid = uid, .block_address = block_id, .block_buffer = buffer },
    } };
    return nfcv_command(cmd);
}

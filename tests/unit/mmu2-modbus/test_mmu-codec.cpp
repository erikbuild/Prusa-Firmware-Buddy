#include <xbuddy_extension/mmu_bridge.hpp>
#include "../../../lib/Marlin/Marlin/src/feature/prusa/MMU2/protocol_logic.h"

#include <catch2/catch_test_macros.hpp>

namespace MMU2 {
void LogResponseMsg(const char *) {}
} // namespace MMU2

namespace buddy::puppies {

PuppyModbus::PuppyModbus() {
}

bool PuppyModbus::read_input_registers_impl(modbus::ServerAddress, uint16_t, std::span<uint16_t>) {
    return false;
}

bool PuppyModbus::write_holding_registers_impl(modbus::ServerAddress, uint16_t, std::span<const uint16_t>) {
    return false;
}

uint16_t returnedRead = 0;
CommunicationStatus returnedStatus = CommunicationStatus::OK;

CommunicationStatus PuppyModbus::read_holding(uint8_t unit, uint16_t *data, uint16_t count, uint16_t address, uint32_t &timestamp_ms, uint32_t max_age_ms) {
    REQUIRE(count == 1); // Current mock implementation doesn't work for more, if your tests need them, you need to improve this
    *data = returnedRead;
    return returnedStatus;
}

CommunicationStatus PuppyModbus::write_holding(uint8_t unit, const uint16_t *data, uint16_t count, uint16_t address, bool &dirty) {
    REQUIRE(count == 1); // Current mock implementation doesn't work for more, if your tests need them, you need to improve this
    return returnedStatus;
}

uint16_t returnedQuery[3] = { 0, 0, 0 };

CommunicationStatus PuppyModbus::read_input(uint8_t unit, uint16_t *data, uint16_t count, uint16_t address, RequestTiming *const timing, uint32_t &timestamp_ms, uint32_t max_age_ms) {
    switch (address) {
    case xbuddy_extension::mmu_bridge::commandInProgressRegisterAddress:
        data[0] = returnedQuery[0];
        data[1] = returnedQuery[1];
        data[2] = returnedQuery[2];
        return returnedStatus;
    default:
        break; // do nothing?
    }
    return returnedStatus;
}

} // namespace buddy::puppies

using namespace modules::protocol;

buddy::puppies::PuppyModbus bus;

void CheckReadRegister(uint8_t address, uint16_t expectedRead) {
    RequestMsg rq(RequestMsgCodes::Read, address);
    auto &mmu = buddy::puppies::mmu;
    mmu.post_read_mmu_register(address);

    // check the control structures
    CHECK(mmu.mmuModbusRq.rw == buddy::puppies::MMU::MMUModbusRequest::RW::read);
    CHECK(mmu.mmuModbusRq.u.read.address == address);

    // prepare simulated modbus comm
    buddy::puppies::returnedRead = expectedRead;
    buddy::puppies::returnedStatus = buddy::puppies::CommunicationStatus::OK;

    // process the message
    CHECK(mmu.refresh(bus) == buddy::puppies::returnedStatus);

    // check control structures
    CHECK(mmu.mmuModbusRq.u.read.value == buddy::puppies::returnedRead);
    CHECK(mmu.mmuModbusRq.u.read.accepted == true);

    // now run ExpectingMessage a couple of times to make sure a valid response got recoded into MMU protocol messages
    ResponseMsg rsp(RequestMsg(RequestMsgCodes::unknown, 0), ResponseMsgParamCodes::unknown, 0);
    uint8_t rawMsg[Protocol::MaxResponseSize()];
    uint8_t rawMsgLen = 0;
    MMU2::ProtocolLogic::ExpectingMessage2(mmu.mmuModbusRq, mmu.mmuQuery, rsp, rq, rawMsg, rawMsgLen);

    if (address < 4) {
        CHECK(rsp.request.code == RequestMsgCodes::Version);
    } else {
        CHECK(rsp.request.code == RequestMsgCodes::Read);
    }
    CHECK(rsp.request.value == address);
    CHECK(rsp.request.value2 == 0);
    CHECK(rsp.paramCode == ResponseMsgParamCodes::Accepted);
    CHECK(rsp.paramValue == expectedRead);
}

TEST_CASE("MMU2-MODBUS read register") {
    for (uint16_t address = 0; address < 256; ++address) {
        CheckReadRegister(address, 256 - address);
    }
}

void CheckWriteRegister(uint8_t address, uint16_t value) {
    RequestMsg rq(RequestMsgCodes::Write, address, value);
    auto &mmu = buddy::puppies::mmu;
    mmu.post_write_mmu_register(address, value);

    // check the control structures
    CHECK(mmu.mmuModbusRq.rw == buddy::puppies::MMU::MMUModbusRequest::RW::write);
    CHECK(mmu.mmuModbusRq.u.write.address == address);
    CHECK(mmu.mmuModbusRq.u.write.value == value);

    // prepare simulated modbus comm
    buddy::puppies::returnedStatus = buddy::puppies::CommunicationStatus::OK;

    // process the message
    CHECK(mmu.refresh(bus) == buddy::puppies::returnedStatus);

    // check control structures
    CHECK(mmu.mmuModbusRq.u.write.accepted == true);

    // now run ExpectingMessage a couple of times to make sure a valid response got recoded into MMU protocol messages
    ResponseMsg rsp(RequestMsg(RequestMsgCodes::unknown, 0), ResponseMsgParamCodes::unknown, 0);
    uint8_t rawMsg[Protocol::MaxResponseSize()];
    uint8_t rawMsgLen = 0;
    MMU2::ProtocolLogic::ExpectingMessage2(mmu.mmuModbusRq, mmu.mmuQuery, rsp, rq, rawMsg, rawMsgLen);

    CHECK(rsp.request.code == RequestMsgCodes::Write);
    CHECK(rsp.request.value == address);
    CHECK(rsp.paramCode == ResponseMsgParamCodes::Accepted);
    CHECK(rsp.paramValue == value);
}

TEST_CASE("MMU2-MODBUS write register") {
    for (uint16_t address = 0; address < 256; ++address) {
        CheckWriteRegister(address, 256 - address);
    }
}

TEST_CASE("MMU2-MODBUS pack-unpack-command") {
    REQUIRE(xbuddy_extension::mmu_bridge::pack_command('X', 0) == 'X');
    REQUIRE(xbuddy_extension::mmu_bridge::pack_command('X', 1) == 256U + 'X');
    REQUIRE(xbuddy_extension::mmu_bridge::pack_command('T', 4) == 4 * 256U + 'T');

    {
        const auto [command, param] = xbuddy_extension::mmu_bridge::unpack_command(256U + 'X');
        REQUIRE(command == 'X');
        REQUIRE(param == 1);
    }

    {
        const auto [command, param] = xbuddy_extension::mmu_bridge::unpack_command(512U + 'T');
        REQUIRE(command == 'T');
        REQUIRE(param == 2);
    }
}

void CheckQuery(uint8_t command, uint8_t param, uint16_t commandStatus, uint16_t pec) {
    RequestMsg rq(RequestMsgCodes::Query, 0);
    auto &mmu = buddy::puppies::mmu;
    mmu.post_query_mmu();

    CHECK(mmu.mmuModbusRq.rw == buddy::puppies::MMU::MMUModbusRequest::RW::query);

    // prepare simulated modbus comm
    buddy::puppies::returnedStatus = buddy::puppies::CommunicationStatus::OK;
    buddy::puppies::returnedQuery[0] = xbuddy_extension::mmu_bridge::pack_command(command, param);
    buddy::puppies::returnedQuery[1] = commandStatus;
    buddy::puppies::returnedQuery[2] = pec;

    // process the message
    CHECK(mmu.refresh(bus) == buddy::puppies::returnedStatus);

    // check control structures - response registers are located aside from this structure
    const auto [rvCommand, rvParam] = xbuddy_extension::mmu_bridge::unpack_command(mmu.mmuQuery.value.cip);
    CHECK(rvCommand == command);
    CHECK(rvParam == param);
    CHECK(mmu.mmuQuery.value.commandStatus == commandStatus);
    CHECK(mmu.mmuQuery.value.pec == pec);

    // now run ExpectingMessage a couple of times to make sure a valid response got recoded into MMU protocol messages
    ResponseMsg rsp(RequestMsg(RequestMsgCodes::unknown, 0), ResponseMsgParamCodes::unknown, 0);
    uint8_t rawMsg[Protocol::MaxResponseSize()];
    uint8_t rawMsgLen = 0;
    MMU2::ProtocolLogic::ExpectingMessage2(mmu.mmuModbusRq, mmu.mmuQuery, rsp, rq, rawMsg, rawMsgLen);

    CHECK(rsp.request.code == (RequestMsgCodes)command);
    CHECK(rsp.request.value == param);
    CHECK(rsp.request.value2 == 0);
    CHECK(rsp.paramCode == (ResponseMsgParamCodes)commandStatus);
    CHECK(rsp.paramValue == pec);
}

TEST_CASE("MMU2-MODBUS query") {
    CheckQuery('T', '0', (uint16_t)ResponseMsgParamCodes::Processing, (uint16_t)ProgressCode::EngagingIdler);
}

void CheckFailedWriteRegister(uint8_t address, uint16_t value) {
    RequestMsg rq(RequestMsgCodes::Write, address, value);
    auto &mmu = buddy::puppies::mmu;
    mmu.post_read_mmu_register(address);

    // check the control structures
    CHECK(mmu.mmuModbusRq.rw == buddy::puppies::MMU::MMUModbusRequest::RW::write);
    CHECK(mmu.mmuModbusRq.u.write.address == address);
    CHECK(mmu.mmuModbusRq.u.write.value == value);

    // prepare simulated modbus comm
    buddy::puppies::returnedStatus = buddy::puppies::CommunicationStatus::ERROR;

    // process the message
    CHECK(mmu.refresh(bus) == buddy::puppies::returnedStatus);

    // check control structures
    CHECK(mmu.mmuModbusRq.u.write.accepted == false);

    // now run ExpectingMessage a couple of times to make sure a valid response got recoded into MMU protocol messages
    // this should end up into either rejected or timeout - and that needs to be distinguished properly
    ResponseMsg rsp(RequestMsg(RequestMsgCodes::unknown, 0), ResponseMsgParamCodes::unknown, 0);
    uint8_t rawMsg[Protocol::MaxResponseSize()];
    uint8_t rawMsgLen = 0;
    MMU2::ProtocolLogic::ExpectingMessage2(mmu.mmuModbusRq, mmu.mmuQuery, rsp, rq, rawMsg, rawMsgLen);

    CHECK(rsp.request.code == RequestMsgCodes::Write);
    CHECK(rsp.request.value == address);
    CHECK(rsp.request.value2 == value);
    CHECK(rsp.paramCode == ResponseMsgParamCodes::Rejected);
    CHECK(rsp.paramValue == value);
}

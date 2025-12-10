/// @file
#include <puppies/mmu.hpp>

#include "timing.h"
#include <cinttypes>
#include <logging/log.hpp>
#include <modbus/modbus.hpp>
#include <mutex>
#include <utility>

LOG_COMPONENT_REF(MMU2);

using Lock = std::unique_lock<freertos::Mutex>;

namespace buddy::puppies {

MMU::MMU(PuppyModbus &bus, uint8_t modbus_address)
    : ModbusDevice(bus, modbus_address) {}

void MMU::post_read_mmu_register(const uint8_t modbus_address) {
    // Post a message to the puppy task and execute the communication handshake there.
    // Fortunately, the MMU code is flexible enough - it doesn't require immediate answers
    std::lock_guard lock(mutex);
    log_debug(MMU2, "post_read_mmu_register %" PRIu8, modbus_address);
    mmuValidResponseReceived = false;
    mmuModbusRq = MMUModbusRequest::make_read_register(modbus_address);
}

void MMU::post_write_mmu_register(const uint8_t modbus_address, const uint16_t value) {
    std::lock_guard lock(mutex);
    log_debug(MMU2, "post_write_mmu_register %" PRIu8, modbus_address);
    mmuValidResponseReceived = false;
    mmuModbusRq = MMUModbusRequest::make_write_register(modbus_address, value);
}

void MMU::post_query_mmu() {
    std::lock_guard lock(mutex);
    log_debug(MMU2, "post_query_mmu");
    mmuValidResponseReceived = false;
    mmuModbusRq = MMUModbusRequest::make_query();
}

void MMU::post_command_mmu(uint8_t command, uint8_t param) {
    std::lock_guard lock(mutex);
    log_debug(MMU2, "post_command_mmu");
    mmuValidResponseReceived = false;
    mmuModbusRq = MMUModbusRequest::make_command(command, param);
}

MMU::MMUModbusRequest MMU::MMUModbusRequest::make_read_register(uint8_t address) {
    MMUModbusRequest request;
    request.u.read.address = address;
    request.u.read.value = 0;
    request.rw = RW::read;
    return request;
}

MMU::MMUModbusRequest MMU::MMUModbusRequest::make_write_register(uint8_t address, uint16_t value) {
    MMUModbusRequest request;
    request.u.write.address = address;
    request.u.write.value = value;
    request.rw = RW::write;
    return request;
}

MMU::MMUModbusRequest MMU::MMUModbusRequest::make_query() {
    MMUModbusRequest request;
    request.u.query.pec = 0;
    request.rw = RW::query;
    return request;
}

MMU::MMUModbusRequest MMU::MMUModbusRequest::make_command(uint8_t command, uint8_t param) {
    MMUModbusRequest request;
    request.u.command.cp = xbuddy_extension::mmu_bridge::pack_command(command, param);
    request.rw = RW::command;
    return request;
}

CommunicationStatus MMU::refresh() {
    Lock lock(mutex);

    // process MMU request
    // the result is written back into the MMUModbusRequest structure
    // by definition of the MMU protocol, there is only one active request-response pair on the bus

    switch (mmuModbusRq.rw) {
    case MMUModbusRequest::RW::read: {
        mmuModbusRq.rw = MMUModbusRequest::RW::read_inactive; // deactivate as it will be processed shortly
        // even if the communication fails, the MMU state machine handles it, it is not required to performs repeats at the MODBUS level
        auto rv = bus.read_holding(unit, &mmuModbusRq.u.read.value, 1, mmuModbusRq.u.read.address, mmuModbusRq.timestamp_ms, 0);
        log_debug(MMU2, "read holding(uni=%" PRIu8 " val=%" PRIu16 " adr=%" PRIu16 " ts=%" PRIu32 " rv=%" PRIu8,
            unit, mmuModbusRq.u.read.value, mmuModbusRq.u.read.address, mmuModbusRq.timestamp_ms, (uint8_t)rv);
        if (rv == CommunicationStatus::OK) {
            mmuModbusRq.u.read.accepted = true; // this is a bit speculative
            mmuValidResponseReceived = true;
        } else {
            mmuModbusRq.u.read.accepted = false;
        }
        return rv;
    }

    case MMUModbusRequest::RW::write: {
        mmuModbusRq.rw = MMUModbusRequest::RW::write_inactive; // deactivate as it will be processed shortly
        bool dirty = true; // force send the MODBUS message
        auto rv = bus.write_holding(unit, &mmuModbusRq.u.write.value, 1, mmuModbusRq.u.write.address, dirty);
        mmuModbusRq.timestamp_ms = last_ticks_ms(); // write_holding doesn't update the timestamp, must be done manually
        log_debug(MMU2, "write holding(uni=%" PRIu8 " val=%" PRIu16 " adr=%" PRIu16 " ts=%" PRIu32 " rv=%" PRIu8,
            unit, mmuModbusRq.u.write.value, mmuModbusRq.u.write.address, mmuModbusRq.timestamp_ms, (uint8_t)rv);
        if (rv == CommunicationStatus::OK) {
            mmuModbusRq.u.write.accepted = true; // this is a bit speculative
            mmuValidResponseReceived = true;
        } else {
            mmuModbusRq.u.write.accepted = false;
        }
        return rv;
    }

    case MMUModbusRequest::RW::query: {
        mmuModbusRq.rw = MMUModbusRequest::RW::query_inactive; // deactivate as it will be processed shortly
        auto rv = bus.read(unit, mmuQuery, 0);
        log_debug(MMU2, "read=%" PRIu8, (uint8_t)rv);
        if (rv == CommunicationStatus::OK) {
            mmuModbusRq.timestamp_ms = mmuQuery.last_read_timestamp_ms;
            mmuValidResponseReceived = true;
        } // otherwise ignore the timestamp, MMU state machinery will time out on its own
        return rv;
    }

    case MMUModbusRequest::RW::command: {
        mmuModbusRq.rw = MMUModbusRequest::RW::command_inactive; // deactivate as it will be processed shortly
        bool dirty = true; // force send the MODBUS message
        log_debug(MMU2, "command");
        auto rv = bus.write_holding(unit, &mmuModbusRq.u.command.cp, 1, xbuddy_extension::mmu_bridge::commandInProgressRegisterAddress, dirty);
        if (rv != CommunicationStatus::OK) {
            const auto [command, param] = xbuddy_extension::mmu_bridge::unpack_command(mmuModbusRq.u.command.cp);
            log_info(MMU2, "command %d failed, param %d", command, param);
            return rv;
        }

        // This is an ugly hack:
        // - First push sent command into local registers' copy - the MMU would respond with it anyway and protcol_logic expects it to be there.
        // - Then read back just the command status (register 254).
        //   Do not issue the Query directly (which would happen by querying register set 253-255)
        //   as it would send a Q0 into the MMU which would replace the command accepted/rejected in register 254.
        //
        // Beware: this command's round-trip may span over 10-20 ms which is close to the MODBUS timeout which is being used for the MMU protocol_logic as well.
        // If the round-trips become longer, MMU protocol_logic must get a larger timeout in mmu_response_received (should cause no harm afterall)
        mmuQuery.value.cip = mmuModbusRq.u.command.cp;
        rv = bus.read_holding(unit, &mmuQuery.value.commandStatus, 1, xbuddy_extension::mmu_bridge::commandStatusRegisterAddress, mmuModbusRq.timestamp_ms, 0);

        if (rv == CommunicationStatus::OK) {
            log_debug(MMU2, "command query result ok");
            // timestamp_ms has been updated by bus.read_holding
            mmuValidResponseReceived = true;
        } // otherwise ignore the timestamp, MMU state machinery will time out on its own
        return rv;
    }

    default:
        return CommunicationStatus::SKIPPED;
    }
}

bool MMU::mmu_response_received(uint32_t rqSentTimestamp_ms) const {
    int32_t td = ~0U;
    {
        std::lock_guard lock(mutex);
        td = ticks_diff(mmuModbusRq.timestamp_ms, rqSentTimestamp_ms);
    }

    if (td >= 0) {
        log_debug(MMU2, "mmu_response_received mmr.ts=%" PRIu32 " rqst=%" PRIu32 " td=%" PRIi32, mmuModbusRq.timestamp_ms, rqSentTimestamp_ms, td);
    } // else drop negative time differences - avoid flooding the syslog
    return mmuValidResponseReceived && td >= 0 && td < PuppyModbus::MODBUS_READ_TIMEOUT_MS;
}

MMU mmu(puppyModbus, std::to_underlying(modbus::ServerAddress::mmu));

} // namespace buddy::puppies

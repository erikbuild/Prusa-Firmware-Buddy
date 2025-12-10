/// @file
#pragma once

#include "PuppyModbus.hpp"
#include <atomic>
#include <freertos/mutex.hpp>
#include <xbuddy_extension/mmu_bridge.hpp>

namespace buddy::puppies {

class MMU final : public ModbusDevice {
public:
    MMU(PuppyModbus &bus, const uint8_t modbus_address);

    // These post requests into the puppy task - only one request is active at a time - MMU protocol_logic behaves that way.
    // I.e. it is not possible/supported to post multiple requests at once and wait for their result.
    void post_read_mmu_register(const uint8_t modbus_address);
    void post_write_mmu_register(const uint8_t modbus_address, const uint16_t value);

    // Virtual MMU registers modelled on the ext board and translated to/from special messages
    // 252: RW current command
    // 253: R command response code and value
    // 254: R either Current_Progress_Code (maps onto MMU register 5) (x) Current_Error_Code (maps onto MMU register 6)
    // Response to this query are 3 16bit numbers:
    // 'T''0', progress code a error code, from that we can compose the MMU's protocol ResponseMsg
    void post_query_mmu();
    void post_command_mmu(uint8_t command, uint8_t param);

    /// @returns true of some MMU response message arrived over MODBUS
    bool mmu_response_received(uint32_t rqSentTimestamp_ms) const;

    struct MMUModbusRequest {
        uint32_t timestamp_ms; ///< timestamp of received response from modbus comm
        union {
            struct ReadRegister {
                uint16_t value; ///< value read from the register
                uint8_t address; ///< register address to read from
                bool accepted;
            } read;
            struct WriteRegister {
                uint16_t value; ///< value to be written into the register
                uint8_t address; ///< register address to write to
                bool accepted;
            } write;
            struct Query {
                uint16_t pec; ///< progress and error code (mutually exclusive)
            } query;
            struct Command {
                uint16_t cp; // command and param combined, because that's what's flying over the wire in a single register
            } command;
        } u;
        enum class RW : uint8_t {
            read = 0,
            write = 1,
            query = 2,
            command = 3,
            inactive = 0x80,
            read_inactive = inactive | read, ///< highest bit tags the request as accomplished - hiding this fact inside the enum makes a usage cleaner
            write_inactive = inactive | write,
            query_inactive = inactive | query,
            command_inactive = inactive | command,
        };
        RW rw = RW::inactive; ///< type of request/response currently active

        static MMUModbusRequest make_read_register(uint8_t address);
        static MMUModbusRequest make_write_register(uint8_t address, uint16_t value);
        static MMUModbusRequest make_query();
        static MMUModbusRequest make_command(uint8_t command, uint8_t param);
    };

    std::atomic<bool> mmuValidResponseReceived;

    /// @note once MMU_response_received returned true, no locking is needed to access the data,
    /// because protocol_logic doesn't issue any other request until this one has been processed
    const MMUModbusRequest &mmu_modbus_rq() const { return mmuModbusRq; }

    MODBUS_REGISTER MMUQueryMultiRegister {
        uint16_t cip; // command in progress
        uint16_t commandStatus; // accepted, rejected, progress, error - simply ResponseMsgParamCodes
        uint16_t pec; // either progressCode (x)or errorCode
    };
    using MMUQueryRegisters = ModbusInputRegisterBlock<xbuddy_extension::mmu_bridge::commandInProgressRegisterAddress, MMUQueryMultiRegister>;

    const MMUQueryRegisters &mmu_query_registers() const { return mmuQuery; }

    // Called from the puppy task.
    CommunicationStatus refresh();

#ifndef UNITTESTS
private:
#endif

    mutable freertos::Mutex mutex;

    MMUQueryRegisters mmuQuery;
    MMUModbusRequest mmuModbusRq;
};

extern MMU mmu;

} // namespace buddy::puppies

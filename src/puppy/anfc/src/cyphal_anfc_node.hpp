#pragma once

#include <cyphal_node.hpp>

// Optimize Cyphal headers for size
#pragma GCC push_options
#pragma GCC optimize("Os")

#include <prusa3d/nfc/command/Request_1_0.h>
#include <prusa3d/common/PortIds_0_1.h>
#include <prusa3d/nfc/Status_1_0.h>
#include <prusa3d/nfc/SetConfig_1_0.h>
#include <honeybee_shared_fault.hpp>

#pragma GCC pop_options

#include "anfc_event_publisher.hpp"

class NFCTask;
using puppy::fault::SharedFault;

namespace anfc::cyphal {

enum Fault {
    // no specific faults yet
    _specific_count,
    _specific_last = _specific_count - 1,

    // Shared faults
    _shared_first = SharedFault::_shared_first, ///< This is a barrier for puppy specific faults

    can = SharedFault::can,
    mcu_overheat = SharedFault::mcu_overheat,
    pcb_overheat = SharedFault::pcb_overheat,
    data_timeout = SharedFault::data_timeout,
    heartbeat_missing = SharedFault::heartbeat_missing,
    unknown = SharedFault::unknown,
};

/// @brief Specify templated conversion.
static inline Fault from_shared(puppy::fault::SharedFault f) { return puppy::fault::from_shared<Fault>(f); };

using ANFCNodeBase = can::cyphal::Node<
    prusa3d_nfc_Status_1_0_Traits, prusa3d_common_PortIds_0_1_MSG_NFC_STATUS,
    prusa3d_nfc_SetConfig_1_0_Traits, prusa3d_common_PortIds_0_1_SRV_NFC_SET_CONFIG,
    Fault,
    CANARD_NODE_ID_UNSET,
    can::cyphal::defaults::WatchNodes,
    can::cyphal::defaults::Notify,
    2, // node_id_request
    15 // MAX_REGISTERS
    >;

/// Task that handles the CAN business logic. Mostly just enqueues requests to the nfc task
class ANFCNode : public ANFCNodeBase {

public:
    using UID = std::array<uint8_t, sizeof(uavcan_node_GetInfo_Response_1_0::unique_id)>;

public:
    ANFCNode(const UID &uid);

public:
    /// Enqueues an event to be published
    /// Will block if the event queue is full
    /// This function is thread-safe
    void enqueue_event(prusa3d_nfc_event_Event_1_0 &event) {
        return event_publisher.enqueue_event(event);
    }

protected:
    void app_init() override;
    void app_tick(int64_t now_us) override;
    void app_tick_pnp(int64_t now_us) override;
    void write_config(const ConfigTraits::Request::Type &config) override;
    void update_status(StatusTraits::Type &data) override;

private: //* Business logic
    ANFCEventPublisher event_publisher;

    // We don't want to deserialize the request actually, we just want to pass the raw data to the NFC task.
    // Deserialized data takes always 256+ bytes, serialized will usually take much less
    struct StubbedRequestSrvTraits : public prusa3d_nfc_command_Request_1_0_Traits {
        using Request = can::RawDataTraits<prusa3d_nfc_command_Request_Request_1_0_Traits>;
    };

    can::cyphal::ServerTraited<StubbedRequestSrvTraits, prusa3d_common_PortIds_0_1_SRV_NFC_REQUEST> request_server;
};

} // namespace anfc::cyphal

#pragma once

// Optimize Cyphal headers for size
#pragma GCC push_options
#pragma GCC optimize("Os")

#include <cyphal_sender_data.hpp>
#include <cyphal_suber_data.hpp>
#include <cyphal_client.hpp>
#include <cyphal_server.hpp>
#include <cyphal_register_dummy.hpp>
#include <cyphal_register.hpp>
#include <cyphal_timesync.hpp>
#include <cyphal_record.hpp>
#include <cyphal_port_list.hpp>
#include <cyphal_task.hpp>

#include <uavcan/node/Mode_1_0.h>
#include <uavcan/node/GetInfo_1_0.h>
#include <uavcan/node/ExecuteCommand_1_3.h>
#include <uavcan/node/Heartbeat_1_0.h>
#include <prusa3d/nfc/command/AcceptEvent_1_0.h>
#include <prusa3d/nfc/command/Request_1_0.h>
#include <prusa3d/nfc/event/Event_1_0.h>
#include <prusa3d/nfc/PortIDs_1_0.h>

#pragma GCC pop_options

#include <freertos/queue.hpp>
#include <move_only_inplace_function.hpp>

namespace anfc::cyphal {

/// Task that handles the CAN business logic. Mostly just enqueues requests to the nfc task
class ANFCNode {

public:
    using UID = std::array<uint8_t, sizeof(uavcan_node_GetInfo_Response_1_0::unique_id)>;

public:
    ANFCNode(const UID &uid);

    /// Initialize CAN node
    void init();

    /// Thread function
    void task();

    /// Enqueues an event to be published
    /// Will block if the event queue is full
    /// This function is thread-safe
    void enqueue_event(prusa3d_nfc_event_Event_1_0 &event);

private: //* Business logic
    /// Events to be published using the event sender
    freertos::Queue<prusa3d_nfc_event_Event_1_0, 32> event_queue;

    /// Increases with each event sent, can overflow, that's okay
    std::atomic<uint16_t> event_id_counter = 0;

    /// ID of the event that is currently being broadcasted, or 0 if no event
    std::atomic<uint16_t> currently_broadcasted_event_id = 0;

    /// How often send event messages until they are accepted
    static constexpr auto event_retransmission_period_ms = 256;

    /// Periodically sends the latest event
    can::cyphal::SenderDataTraited<prusa3d_nfc_event_Event_1_0_Traits, prusa3d_nfc_PortIDs_1_0_MSG_Event> event_sender;

    can::cyphal::ServerTraited<prusa3d_nfc_command_AcceptEvent_1_0_Traits, prusa3d_nfc_PortIDs_1_0_SRV_AcceptEvent> accept_event_server;

    can::cyphal::ServerTraited<prusa3d_nfc_command_Request_1_0_Traits, prusa3d_nfc_PortIDs_1_0_SRV_Request> request_server;

private: //* Scaffolding
    uavcan_node_GetInfo_Response_1_0 get_info_resp; ///< GetInfo response

    /// Mandatory register interface
    can::cyphal::RegisterMachine<12> registers; // Set and get registers

    /// Heartbeat message, mandatory state report
    can::cyphal::SenderDirectTraited<uavcan_node_Heartbeat_1_0_Traits> heartbeat_sender;

    // Last time we sent heartbeat
    uint32_t last_heartbeat = 0;

    ///< Publish list of used ports
    can::cyphal::PortList port_list;

    uavcan_node_Mode_1_0 mode = { .value = uavcan_node_Mode_1_0_INITIALIZATION };

    /// ExecuteCommand server, mandatory command interface
    can::cyphal::ServerTraited<uavcan_node_ExecuteCommand_1_3_Traits> execute_command_server;

    /// GetInfo Server, mandatory device info
    static constexpr const char *name = "cz.prusa3d.pub6.nfc";
    can::cyphal::ServerTraited<uavcan_node_GetInfo_1_0_Traits> get_info_server;
};

} // namespace anfc::cyphal

/// Defined in main.cpp
extern anfc::cyphal::ANFCNode can_node;

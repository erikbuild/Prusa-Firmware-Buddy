#pragma once

#include <span>

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
#include <cyphal_traits_utils.hpp>
#include <salted_app_hash_command_server.hpp>

#include <uavcan/node/Mode_1_0.h>
#include <uavcan/node/GetInfo_1_0.h>
#include <uavcan/node/ExecuteCommand_1_3.h>
#include <uavcan/node/Heartbeat_1_0.h>
#include <prusa3d/nfc/command/AcceptEvent_1_0.h>
#include <prusa3d/nfc/command/Request_1_0.h>
#include <prusa3d/nfc/event/Event_1_0.h>
#include <prusa3d/nfc/PortIDs_1_0.h>
#include <prusa3d/nfc/event/EventID_1_0.h>

#pragma GCC pop_options

#include <utils/atomic_circular_queue.hpp>
#include <freertos/wait_condition.hpp>
#include <freertos/mutex.hpp>
#include <o1heap/o1heap.hpp>

namespace anfc::cyphal {

/// Task that handles the CAN business logic. Mostly just enqueues requests to the nfc task
class ANFCNode {

public:
    using UID = std::array<uint8_t, sizeof(uavcan_node_GetInfo_Response_1_0::unique_id)>;
    using EventID = decltype(prusa3d_nfc_event_EventID_1_0::value);

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
    struct EventRecord {
        /// Serialized data stored in event_queue_heap
        void *serialized_data;

        uint16_t serialized_data_size;

        /// Event ID
        EventID id;
    };

    /// Events to be published using the event sender
    /// The spans are to the event_queue_heap. After sending an event,
    AtomicCircularQueue<EventRecord, uint8_t, 16> event_queue;

    /// Heap of events that are queued to be sent
    O1Heap<1024> event_queue_heap;

    /// For heap allocations and deallocations & event_queue enqueue (the circular queue is thread-safe only if writing from a single task)
    freertos::Mutex event_queue_mutex;

    /// Wait condition for when the event queue is full
    freertos::WaitCondition event_queue_condition;

    /// Increases with each event sent, can overflow, that's okay
    std::atomic<EventID> event_id_counter = 0;

    /// ID of the event that is currently being broadcasted, or 0 if no event
    std::atomic<EventID> currently_broadcasted_event_id = 0;

    /// How often send event messages until they are accepted
    static constexpr auto event_retransmission_period_ms = 256;

    /// Periodically sends the latest event
    /// Work with raw data - we are already getting the events
    can::cyphal::SenderDataTraited<can::RawDataTraits<prusa3d_nfc_event_Event_1_0_Traits>, prusa3d_nfc_PortIDs_1_0_MSG_Event> event_sender;

    can::cyphal::ServerTraited<prusa3d_nfc_command_AcceptEvent_1_0_Traits, prusa3d_nfc_PortIDs_1_0_SRV_AcceptEvent> accept_event_server;

    // We don't want to deserialize the request actually, we just want to pass the raw data to the NFC task.
    // Deserialized data takes always 256+ bytes, serialized will usually take much less
    struct StubbedRequestSrvTraits : public prusa3d_nfc_command_Request_1_0_Traits {
        using Request = can::RawDataTraits<prusa3d_nfc_command_Request_Request_1_0_Traits>;
    };

    can::cyphal::ServerTraited<StubbedRequestSrvTraits, prusa3d_nfc_PortIDs_1_0_SRV_Request> request_server;

private: //* Scaffolding
    uavcan_node_GetInfo_Response_1_0 get_info_resp; ///< GetInfo response

    /// Mandatory register interface
    can::cyphal::RegisterMachine<6> registers; // Set and get registers

    /// Heartbeat message, mandatory state report
    can::cyphal::SenderDirectTraited<uavcan_node_Heartbeat_1_0_Traits> heartbeat_sender;

    // Last time we sent heartbeat
    uint32_t last_heartbeat = 0;

    ///< Publish list of used ports
    can::cyphal::PortList port_list;

    uavcan_node_Mode_1_0 mode = { .value = uavcan_node_Mode_1_0_INITIALIZATION };

    /// ExecuteCommand server, mandatory command interface
    can::cyphal::ServerTraited<uavcan_node_ExecuteCommand_1_3_Traits> execute_command_server;

    can::cyphal::SaltedAppHashCommandServer salted_app_hash_server;

    /// GetInfo Server, mandatory device info
    static constexpr const char *name = "cz.prusa3d.pub6.nfc";
    can::cyphal::ServerTraited<uavcan_node_GetInfo_1_0_Traits> get_info_server;
};

} // namespace anfc::cyphal

/// Defined in main.cpp
extern anfc::cyphal::ANFCNode can_node;

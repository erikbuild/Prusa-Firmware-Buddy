/// \file
#pragma once

#define DO_NOT_CHECK_ATOMIC_LOCK_FREE

// Optimize Cyphal headers for size
#pragma GCC push_options
#pragma GCC optimize("Os")

#include <prusa3d/nfc/event/EventID_1_0.h>
#include <prusa3d/nfc/command/AcceptEvent_1_0.h>
#include <prusa3d/nfc/event/Event_1_0.h>
#include <prusa3d/nfc/PortIDs_1_0.h>

#pragma GCC pop_options

#include <utils/atomic_circular_queue.hpp>
#include <freertos/wait_condition.hpp>
#include <freertos/mutex.hpp>
#include <o1heap/o1heap.hpp>
#include <inplace_function.hpp>
#include <cyphal_server.hpp>
#include <cyphal_port_list.hpp>
#include <cyphal_traits_utils.hpp>

namespace anfc::cyphal {

class ANFCEventPublisher {

public:
    using EventID = decltype(prusa3d_nfc_event_EventID_1_0::value);

public:
    ANFCEventPublisher();

    // TODO: Make registers base class
    void init(can::cyphal::PortList &port_list, auto &registers) {
        port_list.add(sender);
        port_list.add(accept_server);

        sender.add_to_task();
        accept_server.add_to_task();

        registers.add_port_name_set("pub.event", prusa3d_nfc_event_Event_1_0_FULL_NAME_AND_VERSION_, sender.get_port_id());
        registers.add_port_name_set("srv.accept_event", prusa3d_nfc_command_AcceptEvent_1_0_FULL_NAME_AND_VERSION_, accept_server.get_port_id());
    }

    void step();

public:
    /// Enqueues an event to be published
    /// Will block if the event queue is full
    /// This function is thread-safe
    void enqueue_event(prusa3d_nfc_event_Event_1_0 &event);

private:
    struct EventRecord {
        /// Serialized data stored in queue_heap
        void *serialized_data;

        uint16_t serialized_data_size;

        /// Event ID
        EventID id;
    };

    /// Events to be published using the event sender
    /// The spans are to the queue_heap. After sending an event,
    AtomicCircularQueue<EventRecord, uint8_t, 16> queue;

    /// Heap of events that are queued to be sent
    O1Heap<1024> queue_heap;

    /// For heap allocations and deallocations & queue enqueue (the circular queue is thread-safe only if writing from a single task)
    freertos::Mutex mutex;

    /// Wait condition for when the event queue is full
    freertos::WaitCondition condition;

    /// Increases with each event sent, can overflow, that's okay
    std::atomic<EventID> id_counter = 0;

    /// ID of the event that is currently being broadcasted, or 0 if no event
    std::atomic<EventID> currently_broadcasted_event_id = 0;

    /// How often send event messages until they are accepted
    static constexpr auto retransmission_period_ms = 256;

    /// Periodically sends the latest event
    /// Work with raw data - we are already getting the events
    can::cyphal::SenderDataTraited<can::RawDataTraits<prusa3d_nfc_event_Event_1_0_Traits>, prusa3d_nfc_PortIDs_1_0_MSG_Event> sender;

    can::cyphal::ServerTraited<prusa3d_nfc_command_AcceptEvent_1_0_Traits, prusa3d_nfc_PortIDs_1_0_SRV_AcceptEvent> accept_server;
};

} // namespace anfc::cyphal

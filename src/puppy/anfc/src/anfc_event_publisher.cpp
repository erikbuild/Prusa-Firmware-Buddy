#include "anfc_event_publisher.hpp"

#include <cyphal_proto_suber.hpp>
#include <cyphal_proto_sender.hpp>

using namespace can::cyphal;

anfc::cyphal::ANFCEventPublisher::ANFCEventPublisher()
    : accept_server {
        [this](const auto &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
            prusa3d_nfc_command_AcceptEvent_Response_1_0 response;

            // Verify that the accept request is for the currently broadcasted event
            response.ok = (currently_broadcasted_event_id == data.event_id.value);

            accept_server.send_response(response);

            if (response.ok) {
                currently_broadcasted_event_id = 0;
            }
        },
        ProtoSender::send_timeout_default,
        ProtoSuber::multipart_timeout_default,
    } {
}

void anfc::cyphal::ANFCEventPublisher::step() {
    if (currently_broadcasted_event_id == 0) {
        // Stop repeating the previously broadcasted event
        sender.set_period(0);

        // Pop a next event from the queue and start broadcasting it
        if (!queue.isEmpty()) {
            const EventRecord rec = queue.dequeue();

            currently_broadcasted_event_id = rec.id;

            // Copy the data to the sender buffer
            sender.transform_data([&](auto &data) -> can::cyphal::TransformResult {
                memcpy(data.serialized_data.data(), rec.serialized_data, rec.serialized_data_size);
                data.serialized_size = rec.serialized_data_size;
                return TransformResult { .success = true, .significant = true };
            });
            sender.set_period(retransmission_period_ms * 1000);

            // The event data has been copied to the sender -> we can free it from the heap
            {
                std::lock_guard lock { mutex };
                queue_heap.free(rec.serialized_data);
            }

            // Notify tasks blocked on enqueue_event that we've freed up some space
            condition.notify_one();
        }
    }
}

void anfc::cyphal::ANFCEventPublisher::enqueue_event(prusa3d_nfc_event_Event_1_0 &event) {
    event.event_id.value = 0;

    // Prevent assigning event ID 0 (with respect to overflows) - that is a special value
    while (event.event_id.value == 0) {
        event.event_id.value = ++id_counter;
    }

    using EvTraits = prusa3d_nfc_event_Event_1_0_Traits;

    // Note: this buffer is quite big - 256 B
    std::array<uint8_t, EvTraits::serialization_buffer_size_bytes> event_data;
    size_t event_size = event_data.size();

    // Serialize the event - serialized event is likely to take much less space than the struct
    if (EvTraits::serialize(&event, event_data.data(), &event_size) < 0) {
        // Serialization failed, shouldn't happen
        std::abort();
    }

    void *enqueued_data = nullptr;

    // Allocate space for the event data on the event queue heap
    {
        std::unique_lock lock { mutex };
        while (!(enqueued_data = queue_heap.alloc(event_size))) {
            condition.wait(lock);
        }
    }

    // Copy the serialized data from the big buffer on stack to the fitted buffer on the heap
    memcpy(enqueued_data, event_data.data(), event_size);

    const EventRecord rec {
        .serialized_data = enqueued_data,
        .serialized_data_size = uint16_t(event_size),
        .id = event.event_id.value,
    };

    // Enqueue the event
    {
        std::unique_lock lock { mutex };
        while (!queue.enqueue(rec)) {
            condition.wait(lock);
        }
    }
}

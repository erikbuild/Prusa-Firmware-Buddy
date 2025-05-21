

#include "cyphal_anfc_node.hpp"

#include <bsod.h>

#include <nfc.hpp>
#include <hal.hpp>
#include <freertos/timing.hpp>
#include <nfc_task.hpp>

#include <prusa3d/nfc/event/Event_1_0.h>
#include <prusa3d/common/CustomExecuteCommand_1_0.h>

namespace anfc::cyphal {

using namespace can::cyphal;

ANFCNode::ANFCNode(const UID &uid)
    : accept_event_server {
        [this](const auto &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
            prusa3d_nfc_command_AcceptEvent_Response_1_0 response;

            // Verify that the accept request is for the currently broadcasted event
            response.ok = (currently_broadcasted_event_id == data.event_id.value);

            accept_event_server.send_response(response);

            if (response.ok) {
                currently_broadcasted_event_id = 0;
            }
        },
        ProtoSender::send_timeout_default,
        ProtoSuber::multipart_timeout_default,
    }
    , request_server {
        [this](const auto &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
            prusa3d_nfc_command_Request_Response_1_0 response;
            response.status = nfc_task.enqueue_serialized_request(std::span<const uint8_t>(data.serialized_data.data(), data.serialized_size)) ? prusa3d_nfc_command_Request_Response_1_0_OK : prusa3d_nfc_command_Request_Response_1_0_REQUEST_QUEUE_FULL,
            request_server.send_response(response);
        },
        ProtoSender::send_timeout_default,
        ProtoSuber::multipart_timeout_default,
    }
    , execute_command_server {
        [this](const auto &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
            // Avoid copy-initializing the output array - initialize only status and output.count
            uavcan_node_ExecuteCommand_Response_1_3 resp;
            resp.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_BAD_COMMAND;
            resp.output.count = 0;

            switch (data.command) {

            case uavcan_node_ExecuteCommand_Request_1_3_COMMAND_RESTART:
                // Response won't get sent
                hal::reset();
                break;

            case uavcan_node_ExecuteCommand_Request_1_3_COMMAND_EMERGENCY_STOP:
                resp.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_SUCCESS;
                bsod("Cyphal Emergency Stop!"); // Abort everything
                break;

            case prusa3d_common_CustomExecuteCommand_1_0_COMMAND_GET_APP_SALTED_HASH:
                salted_app_hash_server.handle_request(execute_command_server, data, resp);
                break;
            }

            execute_command_server.send_response(resp);
        },
        ProtoSender::send_timeout_default,
        ProtoSuber::multipart_timeout_default,
    }
    , get_info_server {
        [this]([[maybe_unused]] const auto &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
            get_info_server.send_response(get_info_resp);
        },
        ProtoSender::send_timeout_default,
        ProtoSuber::multipart_timeout_default,
    } {

    // Fill in GetInfo response
    {
        memset(&get_info_resp, 0, sizeof(get_info_resp));

        get_info_resp.protocol_version.major = CANARD_CYPHAL_SPECIFICATION_VERSION_MAJOR;
        get_info_resp.protocol_version.minor = CANARD_CYPHAL_SPECIFICATION_VERSION_MINOR;

        memcpy(get_info_resp.unique_id, uid.data(), uid.size());

        get_info_resp.name.count = strlen(name);
        memcpy(&get_info_resp.name.elements, name, get_info_resp.name.count);
    }
}

void ANFCNode::init() {
    // Init subcomponents
    port_list.init();
    registers.init(port_list);

    const auto setup_basic_server = [&](auto &server) {
        port_list.add(server);
        server.add_to_task();
    };

    const auto setup_server = [&](ServerVoid &server, const char *port_name, const char *data_type) {
        setup_basic_server(server);
        registers.add_port_name_set(port_name, data_type, server.get_port_id());
    };

    const auto setup_sender = [&](ProtoSenderPeriodic &sender, const char *port_name, const char *data_type) {
        port_list.add(sender);
        sender.add_to_task();
        registers.add_port_name_set(port_name, data_type, sender.get_port_id());
    };

    setup_basic_server(execute_command_server);
    setup_basic_server(get_info_server);
    setup_server(accept_event_server, "srv.accept_event", prusa3d_nfc_command_AcceptEvent_1_0_FULL_NAME_AND_VERSION_);
    setup_server(request_server, "srv.request", prusa3d_nfc_command_Request_1_0_FULL_NAME_AND_VERSION_);

    setup_sender(event_sender, "pub.event", prusa3d_nfc_event_Event_1_0_FULL_NAME_AND_VERSION_);

    // TODO: ???
    can::cyphal::cyphal_task.set_node_id(2);
}

void ANFCNode::task() {
    while (true) {
        const auto now = ticks_ms();

        // Switch from init to operational mode
        if (mode.value == uavcan_node_Mode_1_0_INITIALIZATION) {
            mode.value = uavcan_node_Mode_1_0_OPERATIONAL;
            // Force send now
            last_heartbeat = 0;
        }

        // Send heartbeat
        if (last_heartbeat == 0 || now - last_heartbeat > uavcan_node_Heartbeat_1_0_MAX_PUBLICATION_PERIOD * 1'000) {
            const uavcan_node_Heartbeat_1_0 data {
                .uptime = static_cast<uint32_t>(now / 1'000),
                .health = { .value = uavcan_node_Health_1_0_NOMINAL },
                .mode = mode,
                /// @todo We could use status code to report something.
                .vendor_specific_status_code = 0,
            };
            heartbeat_sender.send_data(data);
            last_heartbeat = now;
        }

        if (currently_broadcasted_event_id == 0) {
            // Stop repeating the previously broadcasted event
            event_sender.set_period(0);

            // Pop a next event from the queue and start broadcasting it
            if (!event_queue.isEmpty()) {
                const EventRecord rec = event_queue.dequeue();

                currently_broadcasted_event_id = rec.id;

                // Copy the data to the sender buffer
                event_sender.transform_data([&](auto &data) -> can::cyphal::TransformResult {
                    memcpy(data.serialized_data.data(), rec.serialized_data, rec.serialized_data_size);
                    data.serialized_size = rec.serialized_data_size;
                    return TransformResult { .success = true, .significant = true };
                });
                event_sender.set_period(event_retransmission_period_ms);

                // The event data has been copied to the sender -> we can free it from the heap
                {
                    std::lock_guard lock { event_queue_mutex };
                    event_queue_heap.free(rec.serialized_data);
                }

                // Notify tasks blocked on enqueue_event that we've freed up some space
                event_queue_condition.notify_one();
            }
        }

        salted_app_hash_server.step(execute_command_server);

        hal::set_status_led((now / 1024) % 2);
        freertos::delay(1);
    }
}

void ANFCNode::enqueue_event(prusa3d_nfc_event_Event_1_0 &event) {
    event.event_id.value = 0;

    // Prevent assigning event ID 0 (with respect to overflows) - that is a special value
    while (event.event_id.value == 0) {
        event.event_id.value = ++event_id_counter;
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
        std::unique_lock lock { event_queue_mutex };
        while (!(enqueued_data = event_queue_heap.alloc(event_size))) {
            event_queue_condition.wait(lock);
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
        std::unique_lock lock { event_queue_mutex };
        while (!event_queue.enqueue(rec)) {
            event_queue_condition.wait(lock);
        }
    }
}

} // namespace anfc::cyphal

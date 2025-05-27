

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
    : request_server {
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

    setup_basic_server(execute_command_server);
    setup_basic_server(get_info_server);
    setup_server(request_server, "srv.request", prusa3d_nfc_command_Request_1_0_FULL_NAME_AND_VERSION_);

    event_publisher.init(port_list, registers);

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

        event_publisher.step();

        salted_app_hash_server.step(execute_command_server);

        hal::set_status_led((now / 1024) % 2);
        freertos::delay(1);
    }
}

} // namespace anfc::cyphal

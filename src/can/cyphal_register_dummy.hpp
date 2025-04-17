#pragma once

#include "cyphal_proto_suber.hpp"
#include "cyphal_server.hpp"
#include "cyphal_task.hpp"
#include "cyphal_port_list.hpp"

#include <uavcan/_register/Access_1_0.h>
#include <uavcan/_register/List_1_0.h>

namespace can::cyphal {

/**
 * @brief This provides dummy responses to register interface.
 */
class RegisterDummy {
    /// @brief Respond to register list requests with nothing.
    Server<uint8_t, 0, uint8_t, 0> server_list;

public:
    RegisterDummy()
        : server_list(
            ProtoSuber::dummy_deserialize, ProtoSender::dummy_serialize, uavcan_register_List_1_0_FIXED_PORT_ID_,
            [this]([[maybe_unused]] const uint8_t &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
                uint8_t out_data = 0;
                server_list.send_response(out_data); // Response with nothing
            },
            ProtoSender::send_timeout_default, ProtoSuber::multipart_timeout_default) {
    }

    /**
     * @brief Does nothing, just to be compatible with RegisterMachine.
     */
    void set_time_sync([[maybe_unused]] void *time_sync) {}

    /**
     * @brief Add itself to task and to portlist.
     * @note Does not add port name and type registers (not needed for uavcan types).
     * @param port_list node's object publishing used ports
     */
    void init(PortList &port_list) {
        server_list.add_to_task();
        port_list.add(server_list);
    }

    /// @brief Get server port-ID.
    [[nodiscard]] CanardPortID get_server_id() const {
        return server_list.get_server_id();
    }
};
} // namespace can::cyphal


#pragma once

#include <random.h>
#include <timing.h>

#include "cyphal_task.hpp"
#include "cyphal_sender_direct.hpp"
#include "cyphal_suber_call.hpp"

#include <uavcan/pnp/NodeIDAllocationData_2_0.h>

namespace can::cyphal {

/**
 * @brief Plug `n` Play object that requests node ID from allocator.
 */
class PnP {
    Task &cyphal_task; ///< Cyphal task to use
    CanardMicrosecond next_send = 0U; ///< Next time to send request

    CanardNodeID id_giver = CANARD_NODE_ID_UNSET; ///< Node ID of a node that given us our node ID

    /// @brief Request ID from allocator by anonymous message.
    SenderDirect<uavcan_pnp_NodeIDAllocationData_2_0, uavcan_pnp_NodeIDAllocationData_2_0_SERIALIZATION_BUFFER_SIZE_BYTES_> id_request;
    uavcan_pnp_NodeIDAllocationData_2_0 request_data; ///< Request data

    /// @brief Receive ID from allocator.
    SuberCall<uavcan_pnp_NodeIDAllocationData_2_0, uavcan_pnp_NodeIDAllocationData_2_0_EXTENT_BYTES_> id_response;

public:
    /**
     * @brief Plug `n` Play object.
     * @param cyphal_task_ Cyphal task to use
     * @param request_id suggest this ID, we can get any other ID if this is not available
     * @param uid unique ID of this node
     */
    PnP(Task &cyphal_task_, CanardNodeID request_id, const uint8_t uid[sizeof(uavcan_pnp_NodeIDAllocationData_2_0::unique_id)])
        : cyphal_task(cyphal_task_)
        , id_request(cyphal_task_,
              uavcan_pnp_NodeIDAllocationData_2_0_serialize_, uavcan_pnp_NodeIDAllocationData_2_0_FIXED_PORT_ID_,
              CanardTransferKindMessage, CANARD_NODE_ID_UNSET, ProtoSender::send_timeout_default, CanardPrioritySlow)
        , id_response(cyphal_task_,
              uavcan_pnp_NodeIDAllocationData_2_0_deserialize_, uavcan_pnp_NodeIDAllocationData_2_0_FIXED_PORT_ID_,
              [this](const uavcan_pnp_NodeIDAllocationData_2_0 &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
                  if (memcmp(data.unique_id, request_data.unique_id, sizeof(data.unique_id))) { // This is not our ID
                      return;
                  }
                  id_giver = meta.remote_node_id; // Save giver ID
                  cyphal_task.set_node_id(data.node_id.value); // Set our ID
                  id_response.remove_from_task(); // Remove response subscription
              }) {
        request_data.node_id.value = request_id;
        assert(uid != nullptr);
        memcpy(request_data.unique_id, uid, sizeof(request_data.unique_id));
    }

    /**
     * @brief Manage sending requests for node ID.
     * Call this reasonably often to reduce risk of collision, ~100 Hz.
     */
    void loop_tx() {
        if (cyphal_task.is_anonymous()) { // Our ID is not set yet
            if (CanardMicrosecond timestamp = get_timestamp_us(); timestamp > next_send) { // Time to send request
                uint32_t random;
                if (rand_u_secure(&random)) { // Plan next send with truerandom offset
                    id_request.send_data(request_data); // Send request
                    next_send = timestamp + 200 * 1000 + (random % (800 * 1000)); // Send every 200-1000 ms
                } // else try again next loop
            }
        }
    }

    /// @brief Start PnP process.
    void start() {
        id_giver = CANARD_NODE_ID_UNSET;
        cyphal_task.set_node_id(CANARD_NODE_ID_UNSET);
        cyphal_task.whitelist_pnp(&id_request, &id_response);
        id_response.add_to_task();
    }

    /**
     * @brief This node has given us the node ID.
     * @return node ID of the node that given us our node ID
     */
    CanardNodeID get_id_giver() const { return id_giver; }
};

} // namespace can::cyphal

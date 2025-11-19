#pragma once

#include <atomic>

#include <cyphal_server.hpp>

#include <uavcan/node/ExecuteCommand_1_3.h>

namespace can::cyphal {

class SaltedAppHashCommandServer {

public:
    using Server = can::cyphal::ServerTraitedBase<uavcan_node_ExecuteCommand_1_3_Traits>;

public:
    /// Handles the ExecuteCommand request.
    /// Possibly modifies and sends the \p response, but it can also be delayed for some of the step() calls
    /// To be executed from the CAN task
    void handle_request(Server &server, const uavcan_node_ExecuteCommand_Request_1_3 &request, uavcan_node_ExecuteCommand_Response_1_3 &response);

    /// Processes the request and sends the response.
    /// To be executed on the app task.
    void step(Server &server);

private:
    uint32_t requested_salt;
    std::atomic<bool> requested = false;
};

} // namespace can::cyphal

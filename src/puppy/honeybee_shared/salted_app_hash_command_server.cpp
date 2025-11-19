#include "salted_app_hash_command_server.hpp"

#include <device/cmsis.h>
#include <option/bootloader.h>

#include "self_hash.hpp"

using namespace can::cyphal;

bool is_debugger_attached() {
#if defined(STM32H5) || defined(STM32G4)
    return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk);
#elif defined(STM32C0)
    return (DBG->CR & (DBG_CR_DBG_STOP_Msk | DBG_CR_DBG_STANDBY_Msk));
#elif defined(UNITTESTS)
    return false;
#else
    #error
#endif
}

void SaltedAppHashCommandServer::handle_request(Server &server, const uavcan_node_ExecuteCommand_Request_1_3 &request, uavcan_node_ExecuteCommand_Response_1_3 &response) {
    if (request.parameter.count != sizeof(requested_salt)) {
        response.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_BAD_PARAMETER;
        server.send_response(response);
        return;
    }

    if (!BOOTLOADER() || is_debugger_attached()) {
        // Refuse to give hash
        //   - if bootloader not preset (hash is useless, its impossible to flash fw)
        //   - debugger connected - to avoid reflashing the board under debugger
        response.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_NOT_AUTHORIZED;
        server.send_response(response);
        return;
    }

    if (requested.load()) {
        // Already calculating
        response.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_BAD_STATE;
        server.send_response(response);
        return;
    }

    requested_salt = request.parameter.elements[0] | (request.parameter.elements[1] << 8) | (request.parameter.elements[2] << 16) | (request.parameter.elements[3] << 24);
    requested = true;

    // Response will be calculated and sent in step()
}

void SaltedAppHashCommandServer::step(Server &server) {
    if (!requested) {
        return;
    }

    uavcan_node_ExecuteCommand_Response_1_3 response;
    if (self_hash_get(requested_salt, response.output.elements)) {
        response.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_SUCCESS;
        response.output.count = 32;
    } else {
        response.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_INTERNAL_ERROR,
        response.output.count = 0;
    }
    server.send_response(response);
    requested = false;
}

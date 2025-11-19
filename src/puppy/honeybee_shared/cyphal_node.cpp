
#include "cyphal_node.hpp"

#include <bsod.h>
#include <otp.hpp>

LOG_COMPONENT_DEF(Node, logging::Severity::info);

namespace can::cyphal {

void detail::ProtoNode::add_otp_registers(RegisterMachineIface &registers) {
    // Add registers
    registers.add_register(
        "hw_bom_id",
        []([[maybe_unused]] const uavcan_register_Value_1_0 &write, uavcan_register_Value_1_0 &read) {
            uavcan_register_Value_1_0_select_natural8_(&read);
            read.natural8.value.elements[0] = otp_get_bom_id().value_or(0);
            read.natural8.value.count = 1;
        },
        false, true);

    registers.add_register(
        "hw_otp_timestamp",
        []([[maybe_unused]] const uavcan_register_Value_1_0 &write, uavcan_register_Value_1_0 &read) {
            uavcan_register_Value_1_0_select_natural64_(&read);
            read.natural64.value.elements[0] = otp_get_timestamp();
            read.natural64.value.count = 1;
        },
        false, true);

    registers.add_register(
        "hw_raw_datamatrix",
        []([[maybe_unused]] const uavcan_register_Value_1_0 &write, uavcan_register_Value_1_0 &read) {
            uavcan_register_Value_1_0_select_unstructured_(&read);
            serial_nr_t sn;
            read.unstructured.value.count = otp_get_serial_nr(sn);
            memcpy(read.unstructured.value.elements, sn.data(), read.unstructured.value.count);
        },
        false, true);
}

void detail::ProtoNode::init_info(uavcan_node_GetInfo_Response_1_0 &get_info_resp, const char *name, const uint8_t uid[sizeof(uavcan_node_GetInfo_Response_1_0::unique_id)]) {
    get_info_resp.protocol_version.major = CANARD_CYPHAL_SPECIFICATION_VERSION_MAJOR;
    get_info_resp.protocol_version.minor = CANARD_CYPHAL_SPECIFICATION_VERSION_MINOR;

    get_info_resp.hardware_version.major = otp_get_board_revision().value_or(0);
    get_info_resp.hardware_version.minor = otp_get_bom_id().value_or(0);

    /// @note Version number omitted to prevent firmware change on every commit.
    get_info_resp.software_version.major = 0;
    get_info_resp.software_version.minor = 0;
    get_info_resp.software_vcs_revision_id = 0;

    memcpy(get_info_resp.unique_id, uid, sizeof(uavcan_node_GetInfo_Response_1_0::unique_id));

    get_info_resp.name.count = strlen(name);
    memcpy(&get_info_resp.name.elements, name, get_info_resp.name.count);

    get_info_resp.software_image_crc.count = 0;
    get_info_resp.certificate_of_authenticity.count = 0;
}

uint8_t detail::ProtoNode::handle_execute_command_reset() {
    NVIC_SystemReset(); // Silent reset

    while (true) {
    }

    return uavcan_node_ExecuteCommand_Response_1_3_STATUS_SUCCESS; // This will not be sent
}

uint8_t detail::ProtoNode::handle_execute_command_emergency_stop() {
    bsod("Cyphal Emergency Stop!"); // Abort everything

    while (true) {
    }

    return uavcan_node_ExecuteCommand_Response_1_3_STATUS_SUCCESS; // This will not be sent
}

} // namespace can::cyphal

#pragma once

#include <canard.h>

namespace can::cyphal {

/**
 * @brief Tooling for Cyphal task.
 */
class ProtoPortList {
    friend class PortList;
    ProtoPortList *portlist_next = nullptr; ///< To be used exclusively by PortList, multiple access handled by portlist

protected:
    CanardPortID port_id; ///< Cyphal message port-ID

public:
    /**
     * @brief Prototype to be used by PortList.
     * @param port_id_ Cyphal message port-ID
     */
    ProtoPortList(CanardPortID port_id_)
        : port_id(port_id_) {}

    /// @return Cyphal message port-ID
    [[nodiscard]] CanardPortID get_port_id() const {
        return port_id;
    }
};

} // namespace can::cyphal

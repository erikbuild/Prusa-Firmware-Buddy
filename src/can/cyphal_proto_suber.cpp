#include "cyphal_proto_suber.hpp"

#include "cyphal_task.hpp"

using namespace can::cyphal;

void ProtoSuber::add_to_task() {
    cyphal_task.add_suber(*this);
}

void ProtoSuber::remove_from_task() {
    cyphal_task.remove_suber(*this);
}

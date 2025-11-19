#include <cyphal_node_stub.hpp>

static NoDriver driver;
can::cyphal::Task can::cyphal::cyphal_task(driver);

TEST_CASE("CAN::minimal_cyphal_node") {
    // Intentionally leak the node, there are some cleanups that missing and fixing it would be out of scope right now
    Node *node = new Node();
    node->init();
}

#include <cyphal_node_stub.hpp>

static NoDriver driver;
can::cyphal::Task::RxQueue<1> cyphal_task_rx_queue;
can::cyphal::Task can::cyphal::cyphal_task(driver, cyphal_task_rx_queue);

TEST_CASE("CAN::minimal_cyphal_node") {
    // Intentionally leak the node, there are some cleanups that missing and fixing it would be out of scope right now
    Node *node = new Node();
    node->init();
}

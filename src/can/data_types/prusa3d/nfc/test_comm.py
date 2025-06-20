import pycyphal.application
import uavcan.node
import uavcan.pnp
import prusa3d.nfc.event
import prusa3d.nfc.command
import prusa3d.nfc.request
import asyncio
import yaml

req_counter = 0

nfc_node_id = 2


def print_msg(msg):
    if msg is None:
        print("NONE")
    else:
        print(yaml.dump(pycyphal.dsdl.to_builtin(msg)))


async def main():
    node_info = uavcan.node.GetInfo_1.Response(
        software_version=uavcan.node.Version_1(major=1, minor=0),
        name="org.opencyphal.pycyphal.demo.demo_app",
    )

    node = pycyphal.application.make_node(node_info, "dummy.db")
    node.start()

    # Handle ID allocation for the NFC
    if True:
        id_alloc_sub = node.make_subscriber(uavcan.pnp.NodeIDAllocationData_2,
                                            "id_alloc_sub")

        id_alloc_pub = node.make_publisher(uavcan.pnp.NodeIDAllocationData_2,
                                           "id_alloc_pub")

        msg = await id_alloc_sub.get(timeout=2)
        print("ID ASSIGNED")

        if msg is not None and msg.node_id.value == 0:
            await id_alloc_pub.publish(
                uavcan.pnp.NodeIDAllocationData_2(
                    node_id=uavcan.node.ID_1(nfc_node_id),
                    unique_id=msg.unique_id))

    accept_event = node.make_client(
        prusa3d.nfc.command.AcceptEvent_1,
        nfc_node_id,
        prusa3d.nfc.PortIDs_1_0.SRV_AcceptEvent,
    )

    request = node.make_client(
        prusa3d.nfc.command.Request_1_0,
        nfc_node_id,
        prusa3d.nfc.PortIDs_1_0.SRV_Request,
    )

    def req_id():
        global req_counter
        req_counter += 1
        return req_counter

    async def send_request(type, data):
        req = request.dtype.Request()
        pycyphal.dsdl.update_from_builtin(
            req,
            {
                "request_id": req_id(),
                "request": {
                    type: data
                },
            },
        )

        print(str(request.dtype.Request))
        print_msg(req)
        msg = await request(req)
        if msg is None:
            print("No response")
            return

        print(str(request.dtype.Response))
        print_msg(msg)

    async def handle_event(msg, _):
        print(str(type(msg)))
        print_msg(msg)
        await accept_event(
            prusa3d.nfc.command.AcceptEvent_1.Request(event_id=msg.event_id))

    event_sub = node.make_subscriber(prusa3d.nfc.event.Event_1,
                                     prusa3d.nfc.PortIDs_1_0.MSG_Event)
    event_sub.receive_in_background(handle_event)

    await send_request(
        "read_field",
        {
            "field": {
                "tag": 0,
                "section": 1,
                "field": 4,  # Material name
            },
            "value_type": 5,  # String
        },
    )

    await send_request(
        "write_field",
        {
            "field": {
                "tag": 0,
                "section": 1,
                "field": 4,  # Material name
            },
            "value": {
                "string": "Test",
            },
        },
    )

    await send_request(
        "read_field",
        {
            "field": {
                "tag": 0,
                "section": 1,
                "field": 4,  # Material name
            },
            "value_type": 5,  # String
        },
    )

    await asyncio.Future()

    node.close()


try:
    asyncio.run(main())
except KeyboardInterrupt:
    pass

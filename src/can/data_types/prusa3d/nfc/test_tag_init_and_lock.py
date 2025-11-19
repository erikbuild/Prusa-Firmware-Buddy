import pycyphal.application
import uavcan.node
import uavcan.pnp
import prusa3d.nfc.event
import prusa3d.nfc.command
import prusa3d.nfc.request
import asyncio
import yaml
"""
Proxmark commands to revert the reversible part:
Password protecting EAS & AFI & locking DSFID is not reversible

hf 15 slixwritepwd -t read -o 12341234 -n 00000000
hf 15 slixwritepwd -t write -o 12341234 -n 00000000
hf 15 slixwritepwd -t easafi -o 12341234 -n 00000000
hf 15 slixwritepwd -t privacy -o 12341234 -n 0F0F0F0F
hf 15 slixwritepwd -t destroy -o 12341234 -n 0F0F0F0F

hf 15 slixprotectpage -r 00000000 -w 00000000 -i 0 -l 0 -p 0

hf 15 info
"""

req_counter = 0

nfc_node_id = 2

initialized = False

default_timeout = 5

password = [0x12, 0x34, 0x12, 0x34]
protect_bytes = 32


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

    accept_event_client = node.make_client(
        prusa3d.nfc.command.AcceptEvent_1,
        nfc_node_id,
        prusa3d.nfc.PortIDs_1_0.SRV_AcceptEvent,
    )

    request = node.make_client(
        prusa3d.nfc.command.Request_1_0,
        nfc_node_id,
        prusa3d.nfc.PortIDs_1_0.SRV_Request,
    )

    event_sub = node.make_subscriber(prusa3d.nfc.event.Event_1,
                                     prusa3d.nfc.PortIDs_1_0.MSG_Event)

    def req_id():
        global req_counter
        req_counter += 1
        return req_counter

    async def accept_event():
        msg = await event_sub.get(timeout=default_timeout)
        print_msg(msg)
        assert msg, "Expected an event"

        await accept_event_client(
            prusa3d.nfc.command.AcceptEvent_1.Request(event_id=msg.event_id))

        return msg

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
        # print_msg(req)
        msg = await request(req)
        assert msg, "No response"

        # print(str(request.dtype.Response))
        # print_msg(msg)

        # Wait for the request done event
        msg = await accept_event()
        assert msg.data.request_done, "Unexpected event"
        return msg.data.request_done.result

    if True:
        print("Waiting for reader Node ID request...")

        id_alloc_sub = node.make_subscriber(uavcan.pnp.NodeIDAllocationData_2,
                                            "id_alloc_sub")

        id_alloc_pub = node.make_publisher(uavcan.pnp.NodeIDAllocationData_2,
                                           "id_alloc_pub")

        msg = await id_alloc_sub.get(timeout=default_timeout)
        print_msg(msg)
        assert msg

        await id_alloc_pub.publish(
            uavcan.pnp.NodeIDAllocationData_2(
                node_id=uavcan.node.ID_1(nfc_node_id),
                unique_id=msg.unique_id))

    tag_id = None

    if True:
        print("Waiting for the tag detected event...")

        msg = await accept_event()
        assert msg.data.tag_detected

        tag_id = msg.data.tag_detected.tag.value

    if True:
        print("Initializing the tag...")

        await send_request(
            "enable_radio",
            {},
        )

        msg = await send_request(
            "initialize_tag",
            {
                "tag": tag_id,
                "protect_first_num_bytes": 32,
                "password": password,
                "protection_policy": 2,  # write_password
                "best_effort": False,
            },
        )

        assert msg.initialize_tag.error == 255

        print("Writing to the unprotected part...")

        msg = await send_request(
            "raw_write",
            {
                "tag": tag_id,
                "offset": protect_bytes,
                "data": [0, 0, 0, 0],
            },
        )

        assert msg.raw_write.error == 255

        print("Writing to the protected part...")

        msg = await send_request(
            "raw_write",
            {
                "tag": tag_id,
                "offset": 0,
                "data": [0, 0, 0, 0],
            },
        )

        assert msg.raw_write.error == 6

    if True:
        print("Unlocking the tag...")

        msg = await send_request(
            "unlock_tag",
            {
                "tag": tag_id,
                "password": password,
            },
        )

        assert msg.unlock_tag.error == 255

        print("Writing to the protected part (after unlocking)...")

        msg = await send_request(
            "raw_write",
            {
                "tag": tag_id,
                "offset": 0,
                "data": [0, 0, 0, 0],
            },
        )

        assert msg.raw_write.error == 255

    if True:
        print("Re-locking the tag...")

        msg = await send_request(
            "initialize_tag",
            {
                "tag": tag_id,
                "protect_first_num_bytes": 32,
                "password": password,
                "protection_policy": 2,  # write_password
                "best_effort": True,
            },
        )

        assert msg.initialize_tag.error == 255

        print("Writing to the protected part (after relocking)...")

        msg = await send_request(
            "raw_write",
            {
                "tag": tag_id,
                "offset": 0,
                "data": [0, 0, 0, 0],
            },
        )

        assert msg.raw_write.error == 6

    print("Done")
    node.close()


try:
    asyncio.run(main())
except KeyboardInterrupt:
    pass

import pycyphal.application
import uavcan.node
import uavcan.pnp
import prusa3d.nfc.event
import prusa3d.nfc.command
import prusa3d.nfc.request
import asyncio
import yaml
import sys

from typing import List, Dict, Any, ClassVar
from dataclasses import dataclass
from blessed import Terminal
from enum import Enum

nfc_node_id = 2

term = Terminal()


class AwsConfig(Enum):
    NO_AWS = 0
    SLOW_AWS = 1
    MEDIUM_AWS = 2
    FAST_AWS = 3


@dataclass
class Configuration():
    target_amplitude: int = 100
    aws_config: AwsConfig = AwsConfig.NO_AWS
    valid_amplitudes: ClassVar[list[int]] = [
        0, 8, 10, 11, 12, 13, 14, 15, 20, 25, 30, 40, 50, 60, 70, 82, 100
    ]

    def _aws_name_string(self) -> str:
        match self.aws_config:
            case AwsConfig.NO_AWS:
                return "No AWS"
            case AwsConfig.SLOW_AWS:
                return "AWS - slow"
            case AwsConfig.MEDIUM_AWS:
                return "AWS - medium"
            case AwsConfig.FAST_AWS:
                return "AWS - fast"
        assert False, "Unreachable"

    def get_name(self) -> str:
        # WARNING: Don't try to manually inline this if statement into the return statement.
        # The interpreter will parse it correctly and everything will work fine,
        # but our great formatter fails to parse it (even on up to date version).
        mod_name = "OOK" if self.target_amplitude >= 100 else f"ASK({self.target_amplitude})"
        return f"{mod_name} ({self._aws_name_string()})"

    def aws_speed_up(self):
        if self.aws_config.value < AwsConfig.FAST_AWS.value:
            self.aws_config = AwsConfig(self.aws_config.value + 1)

    def aws_speed_down(self):
        if self.aws_config.value > AwsConfig.NO_AWS.value:
            self.aws_config = AwsConfig(self.aws_config.value - 1)

    def amplitude_up(self):
        curr_index = self.valid_amplitudes.index(self.target_amplitude)
        if curr_index < len(self.valid_amplitudes) - 1:
            self.target_amplitude = self.valid_amplitudes[curr_index + 1]

    def amplitude_down(self):
        curr_index = self.valid_amplitudes.index(self.target_amplitude)
        if curr_index > 0:
            self.target_amplitude = self.valid_amplitudes[curr_index - 1]


async def main():
    node_info = uavcan.node.GetInfo_1.Response(
        software_version=uavcan.node.Version_1(major=1, minor=0),
        name="org.opencyphal.pycyphal.demo.demo_app",
    )

    node = pycyphal.application.make_node(node_info, "dummy.db")
    node.start()

    id_alloc_sub = node.make_subscriber(uavcan.pnp.NodeIDAllocationData_2,
                                        "id_alloc_sub")

    id_alloc_pub = node.make_publisher(uavcan.pnp.NodeIDAllocationData_2,
                                       "id_alloc_pub")

    msg = await id_alloc_sub.get(timeout=5)
    if msg is not None:
        await id_alloc_pub.publish(
            uavcan.pnp.NodeIDAllocationData_2(
                node_id=uavcan.node.ID_1(nfc_node_id),
                unique_id=msg.unique_id))

    else:
        print("READER PROBABLY ALREADY ALLOCATED")
    print("READER DETECTED")

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

    def req_id_gen():
        req_counter = 0
        while True:
            yield req_counter
            req_counter += 1

    req_id = req_id_gen()

    async def send_request(type, data):
        req = request.dtype.Request()
        request_id = req_id.__next__()
        pycyphal.dsdl.update_from_builtin(
            req,
            {
                "request_id": request_id,
                "request": {
                    type: data
                },
            },
        )

        msg = await request(req)
        if msg is None:
            print("No response")
        return request_id

    async def handle_event(msg, _):
        await accept_event(
            prusa3d.nfc.command.AcceptEvent_1.Request(event_id=msg.event_id))

    event_sub = node.make_subscriber(prusa3d.nfc.event.Event_1,
                                     prusa3d.nfc.PortIDs_1_0.MSG_Event)

    async def receive_event():
        event = await event_sub.get(0.01)
        if event is not None:
            await handle_event(event, None)
        return event

    request_id = await send_request(
        "enable_radio",
        {},
    )

    # Enable radio done
    event = await receive_event()
    while not event or not event.data.request_done or event.data.request_done.request_id.value != request_id:
        event = await receive_event()

    conf = Configuration(100, AwsConfig.NO_AWS)

    def process_input():
        res = False
        with term.cbreak():
            while True:
                in_key = term.inkey(0.01)
                if in_key.is_sequence and in_key.code == term.KEY_UP:
                    conf.aws_speed_up()
                    res = True
                elif in_key.is_sequence and in_key.code == term.KEY_DOWN:
                    conf.aws_speed_down()
                    res = True
                elif in_key.is_sequence and in_key.code == term.KEY_LEFT:
                    conf.amplitude_down()
                    res = True
                elif in_key.is_sequence and in_key.code == term.KEY_RIGHT:
                    conf.amplitude_up()
                    res = True
                else:
                    break

        return res

    while True:
        print(term.home + term.clear)
        print("Reconfiguring ...")
        request_id = await send_request(
            "set_debug_config",
            {
                "enforce_antenna":
                0,
                "auto_forget_tag":
                True,
                "modulation_settings": [{
                    "target_amplitude": conf.target_amplitude,
                    "aws_config": {
                        "value": conf.aws_config.value
                    }
                }]
            },
        )

        # Configuration done
        event = await receive_event()
        while not event or not event.data.request_done or event.data.request_done.request_id.value != request_id:
            event = await receive_event()

        print(
            f"Antenna reconfigured for {conf.get_name()}. Measure detection distance.",
            flush=True)

        first = True

        while True:
            if process_input():
                break
            # NFC tag detected
            event = await receive_event()
            if event is None:
                continue
            while event.data is None or (event.data is not None
                                         and event.data.tag_detected is None
                                         and event.data.tag_lost is None):
                event = await receive_event()

            if not first:
                print(term.move_up, end="")
            else:
                first = False

            if event.data.tag_detected is not None:
                print(term.green_reverse("Tag detected"), flush=True)
            else:
                print(term.on_darkred("  Tag lost  "), flush=True)

    print("All configuration tested")

    node.close()


try:
    asyncio.run(main())
except KeyboardInterrupt:
    pass

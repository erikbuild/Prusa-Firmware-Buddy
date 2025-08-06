import serial
import time
import crc
from cobs import cobs
import can
import serial.serialutil
import serial.threaded
import asyncio
import serial_asyncio
import struct
import time

# This script acts as a translator between UART messages that encapsulated CAN frames and virtual CAN interface

_BIT_SRV_NOT_MSG = 1 << 25
_BIT_MSG_ANON = 1 << 24
_BIT_SRV_REQ = 1 << 24
_BIT_R23 = 1 << 23
_BIT_MSG_R7 = 1 << 7
SUBJECT_ID_MASK = 2**13 - 1
SERVICE_ID_MASK = 2**9 - 1
PRIORITY_MASK = 7
NODE_ID_MASK = 127

# Encoded messages use CRC16-CCITT (false)
config = crc.Configuration(
    width=16,
    polynomial=0x1021,
    init_value=0xFFFF,
    final_xor_value=0x0000,
)
# The calculator is created from the configuration
crc_calculator = crc.Calculator(config)


def port_to_name(port):
    portNameMapper = {
        8184: 'uavcan.diagnostic.Record.1',
        8166: 'uavcan.pnp.NodeIDAllocationData.1',
        8165: 'uavcan.pnp.NodeIDAllocationData.2',
        7509: 'uavcan.node.Heartbeat.1',
        7510: 'uavcan.node.port.List',
        435: 'uavcan.node.ExecuteCommand.1',
        430: 'uavcan.node.GetInfo.1',
        408: 'uavcan.file.Read.1',
    }
    name = portNameMapper.get(port)
    if name is None:
        return port
    return name


def unpack_bytes(data, n):
    return data[:n], data[n:]


def unpack_unsigned(data, n):
    value, data = unpack_bytes(data, n)
    return int.from_bytes(value, 'little', signed=False), data


def unpack_header(message):
    #unpack CAN ID
    can_id, message = unpack_unsigned(message, 4)
    payload_len, message = unpack_unsigned(message, 1)
    return {
        'can_id': can_id,
        'payload_len': payload_len,
    }, message


def unpack_can_id(identifier):
    priority = (identifier >> 26) & PRIORITY_MASK
    source_node_id = identifier & NODE_ID_MASK
    if identifier & _BIT_SRV_NOT_MSG:
        if identifier & _BIT_R23:
            return {}

        service_id = (identifier >> 14) & SERVICE_ID_MASK
        request_not_response = identifier & _BIT_SRV_REQ != 0
        destination_node_id = (identifier >> 7) & NODE_ID_MASK
        what = 'Request' if request_not_response else 'Response'
        return {
            'prio': priority,
            'what': what,
            'port': port_to_name(service_id),
            'src': source_node_id,
            'dst': destination_node_id,
        }

    if identifier & (_BIT_R23 | _BIT_MSG_R7):
        return {}

    subject_id = (identifier >> 8) & SUBJECT_ID_MASK
    return {
        'prio': priority,
        'what': 'Message',
        'port': port_to_name(subject_id),
        'src': None if identifier & _BIT_MSG_ANON else source_node_id,
        'dst': None,
    }


def unpack_payload(message, payload_len):
    payload, message = unpack_bytes(message, payload_len)
    return payload, message


def unpack_crc16(message):
    crc, data = unpack_unsigned(message, 2)
    return crc, data


def unpack_uart_message(message):
    calculated_crc = crc_calculator.checksum(message[:-2])
    header, message = unpack_header(message)
    payload_len = header.get('payload_len')
    payload, message = unpack_payload(message, payload_len)
    crc, message = unpack_crc16(message)
    return {
        "header": header,
        "payload": payload,
        "crc": {
            "extracted": crc,
            "calculated_crc": calculated_crc
        }
    }


def pack_can_frame(frame):
    """
    Packs CAN data into a byte string with a CRC16 checksum.

    Format: [CAN ID (4B)] [payload length (1B)] [payload (0-64B)] [CRC16 (2B)]
    """
    if len(frame.data) > 64:
        raise ValueError("CAN data payload cannot exceed 64 bytes")

    # For some reason it seems that pycan library stores payload len in dlc instead of the dlc itself...
    payload_for_crc = struct.pack(f'<IB{frame.dlc}s', frame.arbitration_id,
                                  frame.dlc, frame.data)
    calculator = crc.Calculator(
        crc.Configuration(width=16,
                          polynomial=0x1021,
                          init_value=0xFFFF,
                          final_xor_value=0x0000))
    calculated_crc = calculator.checksum(payload_for_crc)
    packed_message = payload_for_crc + struct.pack(f'<H', calculated_crc)
    encoded_message = cobs.encode(packed_message)
    return encoded_message + b'\x00'


def get_printable_bytes(data):
    # Format the bytes into a space-separated hex string
    hex_string = " ".join(f"0x{byte:02X}" for byte in data)
    return hex_string


def get_can_message(frame):
    msg = can.Message(arbitration_id=frame["header"]["can_id"],
                      data=frame["payload"],
                      is_extended_id=True,
                      is_fd=True)
    return msg


def pretty_print_uart_to_vcan(unpacked_frame, original_bytes):
    """Corrected pretty printer."""
    crc_info = unpacked_frame["crc"]
    is_crc_ok = crc_info["calculated_crc"] == crc_info["extracted"]

    print(f"""
(UART->VCAN) ID: {unpacked_frame["header"]["can_id"]} -> {unpack_can_id(unpacked_frame["header"]["can_id"]).get("port")} CRC: {'OK' if is_crc_ok else 'FAIL!'}
""")


def pretty_print_vcan_to_uart(frame):
    print(f"""
(VCAN -> UART) ID: {frame.arbitration_id} ({frame.dlc} B) -> {unpack_can_id(frame.arbitration_id).get("port")}
Payload: {frame.data.hex(' ')}
""")


# Listens to the vcan socket and and transport the accepted message through UART
async def vcan_to_uart_task(serial_transport, bus):
    """Coroutine to read from a CAN socket and write to a serial transport"""
    reader = can.AsyncBufferedReader()
    notifier = can.Notifier(bus, [reader], loop=asyncio.get_running_loop())
    print("vcan_to_uart_task started.")

    try:
        async for frame in reader:
            pretty_print_vcan_to_uart(frame)
            encoded_uart_message = pack_can_frame(frame)

            serial_transport.write(encoded_uart_message)
            await serial_transport.drain()

    except asyncio.CancelledError:
        print("vcan_to_uart_task cancelled.")
    except can.CanError:
        print("Failed to transmit - CANError")
    finally:
        notifier.stop()


# Listens to the uart socket and transport transformed FD CAN frames through vcan
async def uart_to_vcan_task(reader, bus):
    while True:
        try:
            raw_message = await reader.readuntil(b'\x00')

            if raw_message and raw_message.endswith(b'\x00'):
                message = raw_message.removesuffix(b'\x00')
                if message:  # Ensure the message is not empty
                    decoded_message = cobs.decode(message)

                    frame = unpack_uart_message(decoded_message)
                    pretty_print_uart_to_vcan(frame, decoded_message)
                    can_message = get_can_message(frame)
                    bus.send(can_message)
        except serial.SerialException as e:
            print(f"Serial communication failed: {e}")
        except asyncio.IncompleteReadError:
            print("Serial connection lost. Closing task.")
            break  # Exit the loop cleanly
        except cobs.DecodeError as e:
            print(f"COBS decode error: {e}")
        except asyncio.CancelledError:
            print("uart_to_vcan_task cancelled")
            break
        except can.CanError as e:
            print(f"Failed to transmit CAN message: {e}")


async def main():
    bus = can.interface.Bus(bustype='socketcan',
                            channel='vcan0',
                            receive_own_message=False,
                            is_extended_id=True,
                            can_filters=None,
                            fd=True)
    tasks = []
    try:
        reader, writer = await serial_asyncio.open_serial_connection(
            url='/dev/ttyUSB0', baudrate=115200)

        uart_2_vcan_task = asyncio.create_task(uart_to_vcan_task(reader, bus))
        tasks.append(uart_2_vcan_task)
        vcan_2_uart_task = asyncio.create_task(vcan_to_uart_task(writer, bus))
        tasks.append(vcan_2_uart_task)

        print("🚀 Symmetrical CAN-UART gateway running.")

        await asyncio.gather(*tasks)

    except serial.serialutil.SerialException as e:
        print(f"Could not open serial port: {e}")
    finally:
        # Cancel all running tasks
        for task in tasks:
            task.cancel()

        # Wait for all tasks to complete their cancellation
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)

        print("All tasks cancelled. Shutting down CAN bus.")
        bus.shutdown()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nApplication terminated by user.")

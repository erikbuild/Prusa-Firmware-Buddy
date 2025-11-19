# OpenPrintTag reader protocol

## Basic concepts
NFC reads/writes are asynchronous.
1. The master sends a `ReadField`/`WriteField` request.
1. The request will be added to the reader internal queue. This will be confirmed in the `RequestResponse` response.
1. After the request is processed, the reader will broadcast the `Event` message.
1. The master confirms processing the event by sending `AcceptEvent` request. Event broadcast will be repeated until the event is accepted.

See the `test_comm.py` script for a communication example.

## Allocated port space
See `PortIDs.1.0.dsdl` for message IDs
Services 100-110
Messages 1000-1010

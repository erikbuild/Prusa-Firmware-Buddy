Sources for CAN Communication
=============================

This directory contains the sources for the Cyphal - CAN FD communication between Buddy and a Dwarf.

As of now it is very experimental. Don't change files you don't understand and read [Cyphal Guide](https://forum.opencyphal.org/t/the-cyphal-guide/778) instead.


Folder Structure
----------------

### `/src/can/data_types`

Defines data structures used for communication.


#### `/src/can/data_types/public_regulated_data_types`

Public data types for Cyphal. **!!!NEVER!!!** change this folder manually.
It needs to be in sync with the public repo `git@github.com:OpenCyphal/public_regulated_data_types.git`.


#### `/src/can/data_types/prusa3d`

Our custom data types.
Starting with `prusa3d` namespace to avoid conflicts with third party devices, e.g. Hitec UAVCAN servos.

Naming should follow the conventions from [Cyphal Specifications](https://opencyphal.org/specification).

Try to use `uavcan.si.xxx` types whenever possible.
For example `uavcan.si.unit.velocity.Scalar.1.0` will always be in `m/s` and understandable by an external observer.
If you need to use a custom unit for optimization, define `prusa3d.unit.velocity.FurlongsPerFortnight.1.0` that will be `@sealed` and thoroughly comment why it was necessary.

See [Cyphal Design Guidelines](https://forum.opencyphal.org/t/the-cyphal-guide/778/5#interface-design-guidelines-1) for more info.


### `/lib/libcanard`

The [Libcanard](https://github.com/OpenCyphal/libcanard) library for Cyphal.

This folder should probably not be changed manually.
Tweaking this could make our bus incompatible with other Cyphal devices.


Cyphal Classes
--------------

### `can::cyphal::Task`

This is the boss (not manager) of the CAN bus and `libcanard` Cyphal library.
It has its own thread that loops through sent and received data, because libcanard is not thread safe.

It needs an instance of `can::Driver` to access the CAN FD peripheral.

It uses `can::cyphal::ProtoSender`, `can::cyphal::ProtoSenderPeriodic` and `can::cyphal::ProtoSuber` to handle transmit and receive.
Most of these classes have `add_to_task()` and `remove_from_task()` methods to add themselves to the `Task` structures.

There is a linked list of `ProtoSender` classes that is iterated through to find a one that wants to send data.
Data are either marked dirty or its period has passed.
It is not that efficient, but it is a compromise in CPU and RAM usage.
Then the data are pulled from `ProtoSender` and passed through libcanard to its Tx queue.

Data from Tx queue are put into the `can::Driver` when there is a free space.

Suber class has embedded `CanardRxSubscription` structure.
Libcanard holds tree of these and uses them to filter incoming frames.
If complete packet (may be composed of multiple frames) is received, the `user_reference` pointer is used to get to `ProtoSuber` and its callback.


### `can::Driver`

Currently `can::FdcanDriver` implements that for `STM32` `FDCAN` peripheral.
It should work on G4 and MP1.
The `can::Driver` interface is so far missing support for filtering incoming messages.
That will be needed to get priority messages and timing.
It could also reduce some load from the CPU on decoding and discarding unwanted messages.


### `can::cyphal::SenderData` and `can::cyphal::SenderDirect`

These build on top of `can::cyphal::ProtoSender` and provide a way to send data to the bus.
Both are templated with Nunavut transpiled C structure, `SERIALIZATION_BUFFER_SIZE_BYTES` and serialization function.

The size `SERIALIZATION_BUFFER_SIZE_BYTES` is used in `can::Task` to allocate a buffer for serialization.

`SenderData` is for data that are sent periodically.
It holds a copy of the data and periodically publishes it.
User can change the data at any time and choose if it will be sent right away or on the next period.
Note that the transpiled C structures can allocate maximal length of variable length arrays, so it can be small on bus but quite large in RAM.

`SenderDirect` is for data that are sent on demand.
It uses the provided data during the function call.
After that, it can be freed.


### `can::cyphal::SuberCall` and `can::cyphal::SuberData`

These build on top of `can::cyphal::ProtoSuber` and provide a way to receive data from the bus.
Both are templated with Nunavut transpiled C structure, `EXTENT_BYTES` and deserialization function.

The size `EXTENT_BYTES` is used in `can::Task` to allocate a buffer for deserialization.

`SuberCall` uses callback and gives user the deserialized data allocated in the `can::Task` buffer.
The callback is called from `can::Task` thread.
User should not do heavy processing in there to not slow down the `can::Task` thread.
The data cease to be valid when the callback ends, so user needs to copy out what is needed.

`SuberData` is to subscribe to data that are received periodically.
It holds a copy of the data and user can read it at any time.
Note that the transpiled C structures can allocate maximal length of variable length arrays, so it can be small on bus but quite large in RAM.


### `can::cyphal::Server`

This class uses `SuberCall` and `SenderDirect` to implement an RPC server.
When request is received on the bus, a callback is called with the request data.
This class is templated both with request and response structures and functions.

User needs to call `send_response()` to send the response.
The response doesn't need to be sent right from the callback, but it needs to be sent soon, because Cyphal depends on timeouts to repeat the data.


### `can::cyphal::Client`

This class uses `SenderDirect` and `SuberCall` to implement an RPC client.
It sends request on bus and waits for a response.
This class is templated both with request and response structures and functions.

This class doesn't hold neither a copy of the request nor the response data.
It uses a semaphore, which can use something like 80 B.

User can use `call_blocking()` that will wait until a callback with response is called.
User should not do heavy processing in the callback to not slow down the `can::Task` thread.
Same as with `SuberCall`, the data cease to be valid when the callback ends, so user needs to copy out what is needed.


### Traited versions of messagers and services
For the above classes there are traited versions.
Instead of giving all the template parameters and some constructor parameters, there is a traits class that contains them all.
The traits class is generated by an addition to Nunavut templates in `nunavut_c_templates` folder.
Initialization of a client for `prusa3d.splitter.Config.1.0` service would look like this:

```cpp
can::cyphal::ClientTraited<prusa3d_splitter_Config_1_0_Traits,
        prusa3d_common_PortIds_0_1_SRV_SPLITTER_SET_CONFIG>
        splitter_config_client;
```

With constructor that doesn't need serialization and port parameters.
Types with fixed port IDs do not use the port ID template parameter:
```cpp
    can::cyphal::ServerTraited<uavcan_node_ExecuteCommand_1_3_Traits> execute_command_server;
```


### `can::cyphal::PnP`

This class implements Plug and Play device functionality.
The device will start in anonymous mode and will ask for a node ID.
The network has to have at least one allocator that will assign the ID.

While the node is in anonymous mode (`cyphal_task.is_anonymous()`), call `loop_tx()` often.
Until an ID is obtained, the node cannot send or receive anything else, so the common loop can be skipped and only `loop_tx()` can be called.


### `can::cyphal::TimeSync`

This class implements time synchronization client.
It listens for sync messages from one master.
It will lock to first master and will keep its time as long as it transmits.

When locked, this can convert local time and remote time.
The remote time or network time is time of the timesync master.


### `can::cyphal::Record`

This class is used to send logs over CAN.
It is made to fit the buddy `logging::Destination` interface.

It has few internal buffers which can be emptied right away or on next call of `try_send()`.
Call `try_send()` periodically.


### `can::cyphal::PortList`

Provide a list of ports used by the node.
This is useful for Yakut or Yukon to know what data are available on the node.
It uses ports stored in other classes.

### `can::cyphal::RegisterDummy`

This provides dummy response to `uavcan.register.List.1.0`.
It is useful for Yakut or Yukon to silence warnings about missing register server.


### `can::cyphal::RegisterMachine`

This is an almost full register server implementation.
Register is a named storage that can hold array of several types.

`add_register()` creates one register with a callback to write and read the register.
The register is always first written and then read.

`add_port_name_set()` is an optimized set of two constant registers `uavcan.pub.PORT_NAME.id` and `uavcan.pub.PORT_NAME.type`.
This set is used to provide port names and types for non-standard ports to Yakut or Yukon.
Use this with all custom publishers and subscribers.

Registers cannot be removed.
The published list needs to stay constant.


### `can::cyphal::Node`

Serves as a playground to test basic Node mandatory and recommended functions.
Could be removed in the future.

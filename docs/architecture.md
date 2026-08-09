# Architecture

## Package dependency direction

```text
device-specific driver
        |
        v
wirestead_ros
        |
        v
wirestead
```

The Wirestead core remains a ROS-independent plain CMake project. The
`wirestead_ros` package adds only reusable ROS integration behavior. A real
driver parses callback-scoped data directly and publishes a semantic ROS
message.

## Callback shutdown contract

Wirestead invokes receive callbacks on its I/O thread. A lifecycle node must
prevent new callback work before stopping and destroying publishers or parser
state.

Recommended deactivation order:

1. Reject new ROS transmit work.
2. Close the `CallbackGate` to reject new callback work.
3. Stop the Wirestead channel.
4. Wait for admitted callbacks to release their leases.
5. Deactivate ROS publishers and destroy driver state.

Do not call `CallbackGate::close_and_wait()` from a callback holding a lease
from the same gate.

## Generic bridge

The future `wirestead_bridge` package is optional. It will publish raw frames
for diagnostics, proxying, recording, and prototyping. It is not the default
path for production device drivers because it adds a raw DDS hop and payload
copy before protocol parsing.

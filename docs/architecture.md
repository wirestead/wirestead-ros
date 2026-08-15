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

## Diagnostics

A driver should report its link through `report_channel_stats()` rather than
inventing a parallel status system. It maps every `RuntimeStats` field onto a
`diagnostic_updater` status and derives the level from state that is true now:
disconnected is `ERROR`, silent beyond the caller's threshold is `STALE`,
active backpressure is `WARN`.

The staleness threshold is the caller's to supply. How long a gap is too long
depends on the device, and a silent sensor is the failure every other field
reports as healthy - the link stays `Connected` because nothing went wrong, the
data simply stopped.

Cumulative counters - dropped messages, failed sends - deliberately do not
raise the level, or one drop an hour ago would latch a warning for the rest of
the run.

## Generic bridge

The future `wirestead_bridge` package is optional. It will publish raw frames
for diagnostics, proxying, recording, and prototyping. It is not the default
path for production device drivers because it adds a raw DDS hop and payload
copy before protocol parsing.

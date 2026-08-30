# Implementation Status

Status as of 2026-08-09.

## Completed

### Wirestead core

- Added the ROS package manifest for package `wirestead`.
- Recorded the ROS 2 integration architecture, naming, driver usage, and
  release analysis in `docs/ros2_support_analysis.md`.
- Published the change as
  [wirestead/wirestead#586](https://github.com/wirestead/wirestead/pull/586).
- Kept the pull request as a draft. The change is not merged into core `main`.

### Wirestead ROS

- Created the `wirestead_ros` ament package at version `0.1.0`.
- Added `CallbackGate` to prevent driver callbacks from outliving their ROS
  owner during shutdown.
- Added `report_channel_stats()`, mapping a channel's `RuntimeStats` onto a
  `diagnostic_updater` status. The level reflects only present state -
  disconnected is ERROR, active backpressure is WARN - because the drop and
  failure counters are cumulative and would otherwise latch a warning for the
  rest of the run over a single old drop. Extended 2026-08-15 with an optional
  `stale_after`: a link that is Connected but has stopped receiving reports
  STALE, which is the failure every other field reports as healthy. The
  threshold is a parameter because only the integrator knows their device's
  slowest interval.
- Added `serial_line_driver`, the reference driver for third-party authors: a
  lifecycle node that parses a line protocol on the callback-scoped view,
  stamps from `MessageContext::received_at()`, orders shutdown through
  `CallbackGate`, and reports the link on `/diagnostics`. It publishes a
  semantic `sensor_msgs/Temperature` rather than raw bytes, so there is no
  extra DDS hop in the data path.
- Added a pseudo-terminal test for that driver, covering the contracts the
  documents assert: a line becomes one semantic message with the configured
  frame, unparseable input is dropped without stopping the driver, activation
  succeeds with no device present, and repeated activate/deactivate cycles are
  clean. The node moved into `serial_line_driver.hpp` so the test can drive its
  transitions directly.
- Added installable CMake targets, package exports, tests, source-workspace
  metadata, and CI. CI covers Jazzy on Ubuntu 24.04 and, since Wirestead's Boost
  minimum dropped to 1.74, Humble on Ubuntu 22.04. Both rows are required.
- Published the initial implementation to the `wirestead-ros` `main` branch.
- Made the development CI build and install Wirestead core `main` before the
  ROS package. The unresolved `wirestead` rosdep key is skipped temporarily.

## Validation

- Wirestead core Linux, macOS, and Windows builds passed.
- Core unit, integration, end-to-end, memory-safety, formatting, install and
  consume, and CodeQL checks passed on pull request 586.
- `wirestead_ros` produced 68 test results with no failures locally.
- An installed downstream consumer found and linked
  `wirestead_ros::wirestead_ros` successfully.
- The public Jazzy workflow completed successfully in
  [wirestead-ros Actions run 31307946464](https://github.com/wirestead/wirestead-ros/actions/runs/31307946464).
- Humble was added to the same workflow on 2026-08-16 and passed: the core
  builds against Ubuntu 22.04's Boost 1.74, `wirestead_ros` builds against it,
  and every functional test passes. Only `uncrustify` differs, because Humble's
  version wants different line wrapping than Jazzy's, so it is excluded on that
  row rather than reformatting the source away from the primary target.

## Current Source-Workspace Use

Until a released Wirestead core package is available through rosdep, clone
this repository and import `wirestead_ros.repos`. That manifest follows core
`main`; install core with plain CMake, skip the unresolved `wirestead` rosdep
key, and then build `wirestead_ros` with colcon. The exact commands are in the
repository [README](../README.md#source-workspace).

Real device drivers should depend on `wirestead_ros`, link Wirestead directly,
and publish semantic standard or device-specific ROS messages. An optional raw
byte bridge is not part of the current package and must not add an extra DDS hop
to production driver data paths.

## Not Yet Included

- rosdistro source entries for `wirestead` and `wirestead_ros`, which are the
  REP-144 naming review and the prerequisite the next item links to.
- `ros2-gbp/wirestead-release` and `ros2-gbp/wirestead-ros-release`, requested
  from the `ros2-gbp` organization rather than created here.
- Bloom registration and ROS build-farm binary packages.
- Planned `wirestead_msgs` and `wirestead_bridge` packages.

Steps 1 to 3 of the registration order below are done as of 2026-08-16: the
core manifest pull request merged as
[wirestead/wirestead#586](https://github.com/wirestead/wirestead/pull/586),
core v0.9.4 was the first release to carry `package.xml`, v0.9.5 the first one
a build farm can actually build, and v0.9.6 the first one whose Debian is
discoverable and does not drag `libboost-all-dev` into its runtime
dependencies. `wirestead_ros.repos` resolves v0.9.6 rather than core `main`.
CI stays on core `main` on purpose, to keep catching integration regressions
between releases. What remains is the rosdistro entries, the `ros2-gbp` release repositories,
and Bloom.

## Registration Order

1. Review and merge core pull request 586.
2. Include the core manifest in the next naturally scheduled Wirestead release;
   do not recreate or modify v0.9.3.
3. Pin `wirestead_ros.repos` to that immutable core tag and repeat clean Jazzy
   source and installed-consumer validation.
4. Add rosdistro `source` entries for both repositories - the REP-144 naming
   review, and the link the next step requires.
5. Request the release team and the two `ros2-gbp` release repositories.
6. Release the core package `wirestead` with Bloom and verify
   `ros-jazzy-wirestead` in `ros-testing`.
7. Release the integration package `wirestead_ros` and verify
   `ros-jazzy-wirestead-ros` in `ros-testing`.

The complete Bloom commands, repository names, and naming rationale are in
[ROS Release Plan](releasing.md).

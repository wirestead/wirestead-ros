# Wirestead ROS

ROS 2 integration for the
[Wirestead](https://github.com/wirestead/wirestead) asynchronous C++
communication library.

The repository is intentionally split from the ROS-independent Wirestead core.
Real device drivers link Wirestead directly through the `wirestead_ros` package
and publish semantic ROS messages. Raw byte bridge packages will remain
optional so production drivers do not pay for an unnecessary DDS hop.

## Packages

| Package | Status | Purpose |
| --- | --- | --- |
| `wirestead_ros` | Initial | Driver integration library: lifecycle-safe callback utilities, `/diagnostics` mapping, and a reference driver |
| `wirestead_msgs` | Planned | Interfaces for the optional generic bridge |
| `wirestead_bridge` | Planned | Serial, TCP, UDP, and UDS bridge nodes |

Wirestead core is expected to be released separately as the ROS package
`wirestead`, producing the Debian package `ros-<distro>-wirestead`.

### What `wirestead_ros` provides today

- `CallbackGate` - admits and drains transport callbacks so none outlives the
  ROS objects it publishes through.
- `report_channel_stats()` - maps a channel's `RuntimeStats` onto
  `diagnostic_updater`, including a `STALE` level for a link that is connected
  but has stopped receiving.
- `serial_line_driver` - the reference driver for third-party authors: a
  lifecycle node that parses on the callback-scoped view, stamps from the
  payload's arrival time, orders shutdown through `CallbackGate`, and publishes
  a semantic `sensor_msgs/Temperature` rather than raw bytes. Its lifecycle and
  I/O are covered by a pseudo-terminal test.

## Source workspace

Until the core package is available from the ROS package repository, install
Wirestead core with plain CMake and then build the ROS package. The manifest
resolves the released core tag, so a source workspace builds what the ROS
release describes; CI separately tracks core `main`, which is what catches
integration regressions before the next tagged release.

```bash
mkdir -p ~/wirestead_ws/src
cd ~/wirestead_ws
git clone https://github.com/wirestead/wirestead-ros.git src/wirestead-ros
vcs import src < src/wirestead-ros/wirestead_ros.repos

cmake -S src/wirestead -B build/wirestead-core \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/wirestead-core" \
  -DWIRESTEAD_BUILD_STATIC=OFF \
  -DWIRESTEAD_BUILD_TESTS=OFF
cmake --build build/wirestead-core -j2
cmake --install build/wirestead-core

source /opt/ros/jazzy/setup.bash
export CMAKE_PREFIX_PATH="$PWD/install/wirestead-core:$CMAKE_PREFIX_PATH"
rosdep install --from-paths src/wirestead-ros/wirestead_ros \
  --ignore-src --rosdistro jazzy --skip-keys wirestead -r -y
colcon build --base-paths src/wirestead-ros/wirestead_ros --symlink-install
```

The `--skip-keys wirestead` exception is temporary. Remove it when Wirestead is
available through rosdistro. Release manifests must replace `main` with an
immutable core tag before a public release.

The completed implementation, validation results, and remaining registration
work are tracked in [Implementation Status](docs/implementation_status.md).

## Supported ROS distributions

- ROS 2 Jazzy on Ubuntu 24.04 is the primary target and the required CI row.
- ROS 2 Humble on Ubuntu 22.04 builds and tests green in CI. Wirestead lowered
  its Boost minimum to 1.74, which is what Ubuntu 22.04 supplies, so the
  dependency that used to rule Humble out is gone. The row is still marked
  experimental until it has been stable for a while.
- ROS 2 Lyrical support requires a separate Ubuntu 26.04 CI result before
  release.

## License

Apache License 2.0. See [LICENSE](LICENSE).

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
| `wirestead_ros` | Initial | Driver integration library and lifecycle-safe callback utilities |
| `wirestead_msgs` | Planned | Interfaces for the optional generic bridge |
| `wirestead_bridge` | Planned | Serial, TCP, UDP, and UDS bridge nodes |

Wirestead core is expected to be released separately as the ROS package
`wirestead`, producing the Debian package `ros-<distro>-wirestead`.

## Source workspace

Until the core package is available from the ROS package repository, install
Wirestead core from its current development branch with plain CMake and then
build the ROS package. The development manifest tracks core `main` so CI catches
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

- ROS 2 Jazzy on Ubuntu 24.04 is the initial development target.
- ROS 2 Lyrical support requires a separate Ubuntu 26.04 CI result before
  release.
- ROS 2 Humble is not currently targeted because its system Boost version is
  below Wirestead's minimum requirement.

## License

Apache License 2.0. See [LICENSE](LICENSE).

# Contributing

## Build

Source ROS and make an installed Wirestead package available through the same
workspace or `CMAKE_PREFIX_PATH`, then run:

```bash
colcon build --symlink-install --event-handlers console_direct+
colcon test
colcon test-result --verbose
```

## Scope

- Keep the Wirestead core ROS-independent.
- Prefer standard ROS messages in real device drivers.
- Keep raw byte bridge interfaces optional.
- Do not add unbounded queues or blocking sends to executor callbacks.
- Preserve callback payload ownership before work leaves the Wirestead I/O
  callback.

## Commits

Use Conventional Commits:

```text
<type>[optional scope]: <description>
```

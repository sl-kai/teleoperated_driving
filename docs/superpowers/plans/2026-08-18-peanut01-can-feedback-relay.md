# Peanut01 CAN Feedback Relay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build and deploy a C++ ROS 2 sidecar which reads only recognized feedback frames from `can0` and publishes the existing JSON contract on `/vehicle/can/raw`.

**Architecture:** A new `ament_cmake` package owns strict frame filtering and JSON serialization. The current vehicle build installs it, while a host-network Compose sidecar runs it in ROS domain 0 without restarting any existing vehicle container.

**Tech Stack:** C++17, ROS 2 Humble (`rclcpp`, `std_msgs`), Linux SocketCAN, GoogleTest, Docker Compose.

---

### Task 1: Add a Testable C++ Envelope Library

**Files:**
- Create: `src/tod_can_feedback_relay/package.xml`
- Create: `src/tod_can_feedback_relay/CMakeLists.txt`
- Create: `src/tod_can_feedback_relay/include/tod_can_feedback_relay/can_frame_envelope.hpp`
- Create: `src/tod_can_feedback_relay/src/can_frame_envelope.cpp`
- Create: `src/tod_can_feedback_relay/test/test_can_frame_envelope.cpp`

- [ ] Write a failing GoogleTest suite. It must accept only `0x1A1`, `0x1A2`, `0x1A3`, `0x1A4`, `0x401`; reject `0x123`, extended, RTR, error, and non-8-byte frames; assert exact output for a 0x1A2 frame:

```cpp
EXPECT_EQ(to_json(frame, 42, "can0"),
  R"({"stamp_ns":42,"interface":"can0","id":418,"id_hex":"0x1A2","is_extended":false,"is_rtr":false,"is_error":false,"dlc":8,"data_hex":"891040050005011B"})");
```

- [ ] Run `colcon test --packages-select tod_can_feedback_relay`; expect failure before the package exists.
- [ ] Define an `ament_cmake` package with `rclcpp`, `std_msgs`, and `ament_cmake_gtest`; build `can_frame_envelope` and `tod_can_feedback_relay` targets.
- [ ] Implement a pure `CanFrame` formatter. Field order must be `stamp_ns`, `interface`, `id`, `id_hex`, `is_extended`, `is_rtr`, `is_error`, `dlc`, `data_hex`; use uppercase `0x%03X` and 16 uppercase data hex digits. No sockets or I/O in this library.
- [ ] Run `colcon test --packages-select tod_can_feedback_relay` and `colcon test-result --verbose`; expect all tests pass.
- [ ] Commit: `git add src/tod_can_feedback_relay && git commit -m "feat: add Peanut01 CAN feedback envelope"`.

### Task 2: Implement the Read-Only Relay Node

**Files:**
- Create: `src/tod_can_feedback_relay/src/can_feedback_relay_node.cpp`
- Modify: `src/tod_can_feedback_relay/CMakeLists.txt`
- Create: `tests/test_can_feedback_relay_contract.py`

- [ ] Add declared parameters `interface` defaulting to `can0` and `topic` defaulting to `/vehicle/can/raw`; publish `std_msgs::msg::String` with reliable QoS depth 500.
- [ ] Open `PF_CAN` / `SOCK_RAW | SOCK_CLOEXEC`, install `CAN_RAW_FILTER` for the five IDs, set `CAN_RAW_RECV_OWN_MSGS` to zero, bind to `if_nametoindex(interface)`, and use only `poll()` plus `recv()` to obtain frames.
- [ ] On socket, poll, or receive failure, log, close the descriptor, then retry after 100, 200, 400, 800, and at most 1000 ms. Never synthesize a feedback message.
- [ ] Write a failing Python contract test requiring `PF_CAN`, `SOCK_RAW`, `CAN_RAW_FILTER`, `CAN_RAW_RECV_OWN_MSGS`, and `/vehicle/can/raw`, and rejecting `send(`, `sendto(`, `sendmsg(`, `write(`, CAN command IDs, and control-command ROS topics.
- [ ] Run `colcon test --packages-select tod_can_feedback_relay`, `colcon test-result --verbose`, and `pytest -q tests/test_can_feedback_relay_contract.py`; expect all pass.
- [ ] Commit: `git add src/tod_can_feedback_relay tests/test_can_feedback_relay_contract.py && git commit -m "feat: add read-only SocketCAN feedback relay"`.

### Task 3: Build and Constrain the Sidecar

**Files:**
- Modify: `docker/dockerfile`
- Modify: `docker-compose.yaml`

- [ ] After the vehicle workspace build, run `colcon test --packages-select tod_can_feedback_relay && colcon test-result --verbose` in the vehicle builder stage.
- [ ] Add service `tod_can_feedback_relay` using the vehicle image and build target. It must use host networking, drop all capabilities then add only `NET_RAW`, set `read_only: true`, use tmpfs for `/tmp` and the ROS log directory, restart unless stopped, set `ROS_DOMAIN_ID=0` and `CAN_INTERFACE=can0`, and command the installed relay executable.
- [ ] Do not add `privileged`, `/dev` mounts, `/userdata` mounts, or dependencies that restart `tod_vehicle`, `minguo`, or `peanut01-drivers`.
- [ ] Run `pytest -q` and `docker compose config --quiet`; expect all tests and Compose validation pass.
- [ ] Commit: `git add docker/dockerfile docker-compose.yaml tests/test_can_feedback_relay_contract.py && git commit -m "feat: deploy CAN feedback relay sidecar"`.

### Task 4: Deploy Without Arming Actuation

**Files:**
- Modify: vehicle checkout at `/userdata/teleoperated_driving` only after the implementation is available there.

- [ ] Confirm the running bridge reports `enable_actuation=false`; stop deployment if true.
- [ ] Build only the relay on the ARM64 vehicle host: `docker compose build tod_can_feedback_relay`.
- [ ] Start only it: `docker compose up -d --no-deps tod_can_feedback_relay`. Never run unqualified `docker compose up`.
- [ ] In ROS domain 0, verify `/vehicle/can/raw` has exactly one relay publisher, one existing `peanut01_control_target` subscriber, and produces a valid recognized JSON envelope.
- [ ] Verify bridge diagnostics show finite `mcu_stat1_age_ms`, `mcu_stat2_age_ms`, `mcu_error_age_ms`, and `eps_status1_age_ms` while actuation remains disabled. Do not request autonomous mode, publish controls, or test movement.

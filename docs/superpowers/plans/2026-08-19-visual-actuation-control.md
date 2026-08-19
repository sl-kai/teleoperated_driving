# Visual Real-Actuation Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a lower-left Visual power control that safely requests real vehicle actuation and displays the vehicle supervisor's confirmed `DISABLED`, `ARMING`, `ACTIVE`, or `FAULT` state.

**Architecture:** A generated service carries enable/disable requests through a dedicated pair of existing-style TCP service forwarding nodes. The Peanut01 bridge delegates requests to its existing supervisor and publishes a generated state message through the normal vehicle-to-operator UDP data transport. Visual owns a focused async service client and a state subscription component; the UI renders only fresh vehicle-confirmed state.

**Tech Stack:** ROS 2 Humble, rosidl messages/services, rclpy vehicle bridge, rclcpp service forwarding, generic TOD UDP data transport, C++17, Dear ImGui, pytest contract tests.

---

## File Structure

- `src/tod_msgs/tod_vehicle_msgs/msg/ActuationControlState.msg`: generated state constants and feedback payload.
- `src/tod_msgs/tod_vehicle_msgs/srv/SetActuationEnabled.srv`: generated request/response contract.
- `work/peanut01_control_bridge.py`: vehicle service endpoint and periodic supervisor state publisher.
- `src/tod_network/tod_communication_interface/src/operator/actuation_control_service_forwarder.cpp`: operator TCP service endpoint.
- `src/tod_network/tod_communication_interface/src/vehicle/actuation_control_service_listener.cpp`: vehicle TCP service endpoint.
- `config/config/package_config/tod_communication_interface/services.yaml`: reserved service ports 60500/60501.
- `config/config/package_config/tod_data_interface/{operator,vehicle}_config.yaml`: state receiver/sender on UDP port 60012.
- `config/config/remappings.yaml`: bridge, data transport, and Visual topic/service wiring.
- `src/tod_operator_interface/tod_visual/src/tod_gl/.../actuation_*_component.*`: thread-safe state subscription and async service request ownership.
- `src/tod_operator_interface/tod_visual/src/tod_applications/visual/application_layer/include/drive_info_layer.hpp`: lower-left fixed-size button, hold interaction, progress ring, state text, and stale handling.
- `tests/test_visual_actuation_control.py`: cross-layer contract and interaction regression tests.

### Task 1: Define Generated Control Contracts

**Files:**
- Create: `src/tod_msgs/tod_vehicle_msgs/msg/ActuationControlState.msg`
- Create: `src/tod_msgs/tod_vehicle_msgs/srv/SetActuationEnabled.srv`
- Modify: `src/tod_msgs/tod_vehicle_msgs/CMakeLists.txt`
- Test: `tests/test_visual_actuation_control.py`

- [ ] **Step 1: Write the failing interface contract test**

Add assertions that the message defines the four numeric constants, header,
state, enable request, and reason, and that the service defines `enable`,
`accepted`, `current_state`, and `reason`. Assert both files appear in
`rosidl_generate_interfaces`.

```python
def test_generated_actuation_interfaces_are_registered():
    state = STATE_MSG.read_text(encoding="utf-8")
    service = CONTROL_SRV.read_text(encoding="utf-8")
    cmake = VEHICLE_MSGS_CMAKE.read_text(encoding="utf-8")
    for token in ("DISABLED=0", "ARMING=1", "ACTIVE=2", "FAULT=3",
                  "std_msgs/Header header", "uint8 state",
                  "bool enable_requested", "string reason"):
        assert token in state
    for token in ("bool enable", "bool accepted", "uint8 current_state",
                  "string reason"):
        assert token in service
    assert '"msg/ActuationControlState.msg"' in cmake
    assert '"srv/SetActuationEnabled.srv"' in cmake
```

- [ ] **Step 2: Run the test and verify RED**

Run: `python -m pytest tests/test_visual_actuation_control.py -q`

Expected: FAIL because both generated interface files are absent.

- [ ] **Step 3: Add the message and service definitions**

```text
# ActuationControlState.msg
uint8 DISABLED=0
uint8 ARMING=1
uint8 ACTIVE=2
uint8 FAULT=3
std_msgs/Header header
uint8 state
bool enable_requested
string reason
```

```text
# SetActuationEnabled.srv
bool enable
---
bool accepted
uint8 current_state
string reason
```

Register the message and service in `tod_vehicle_msgs/CMakeLists.txt` and keep
`std_msgs` in the generated interface dependencies.

- [ ] **Step 4: Run the test and verify GREEN**

Run: `python -m pytest tests/test_visual_actuation_control.py -q`

Expected: PASS.

- [ ] **Step 5: Commit the interface contract**

```bash
git add tests/test_visual_actuation_control.py \
  src/tod_msgs/tod_vehicle_msgs/msg/ActuationControlState.msg \
  src/tod_msgs/tod_vehicle_msgs/srv/SetActuationEnabled.srv \
  src/tod_msgs/tod_vehicle_msgs/CMakeLists.txt
git commit -m "feat: define real-actuation control interfaces"
```

### Task 2: Expose Vehicle Supervisor Control And State

**Files:**
- Modify: `work/peanut01_control_bridge.py`
- Modify: `tests/test_visual_actuation_control.py`
- Modify: `tests/test_peanut01_control_bridge.py`

- [ ] **Step 1: Write failing bridge contract tests**

Assert the bridge imports `ActuationControlState` and `SetActuationEnabled`,
creates `set_actuation_enabled`, publishes
`/vehicle/interface/peanut01/actuation_control_state`, maps every supervisor
state through generated constants, publishes at runtime, and modifies only
`shared.requested_enable` under its lock in the service callback.

```python
def test_bridge_exposes_service_and_confirmed_state():
    bridge = BRIDGE.read_text(encoding="utf-8")
    for token in (
        "ActuationControlState", "SetActuationEnabled",
        '"set_actuation_enabled"',
        '"/vehicle/interface/peanut01/actuation_control_state"',
        "self.shared.requested_enable = request.enable",
        "self.actuation_state_publisher.publish(message)",
    ):
        assert token in bridge
```

- [ ] **Step 2: Run bridge tests and verify RED**

Run: `python -m pytest tests/test_visual_actuation_control.py tests/test_peanut01_control_bridge.py -q`

Expected: FAIL because the bridge has no dedicated service or state publisher.

- [ ] **Step 3: Implement the service and periodic feedback**

Extend `SharedInputs` with `actuation_state` and `actuation_reason`. The service
callback must only enqueue the requested boolean and return the latest locked
snapshot:

```python
def on_set_actuation_enabled(self, request, response):
    with self.shared.lock:
        self.shared.requested_enable = request.enable
        response.accepted = True
        response.current_state = self.shared.actuation_state
        response.reason = self.shared.actuation_reason
    return response
```

Map `State.DISABLED`, `State.ARMING`, `State.ACTIVE`, and `State.FAULT` to
generated constants. On every bridge timer cycle, update the locked shared
snapshot and publish a stamped `ActuationControlState`. Publishing every cycle
is intentional: the small message is the freshness heartbeat consumed by
Visual. Preserve the existing parameter callback as a compatible manual
fallback and preserve startup-disabled behavior.

- [ ] **Step 4: Run bridge tests and verify GREEN**

Run: `python -m pytest tests/test_visual_actuation_control.py tests/test_peanut01_control_bridge.py tests/test_peanut01_control_supervisor.py -q`

Expected: PASS, including the existing fault-latch and safety-gate tests.

- [ ] **Step 5: Commit vehicle behavior**

```bash
git add work/peanut01_control_bridge.py \
  tests/test_visual_actuation_control.py tests/test_peanut01_control_bridge.py
git commit -m "feat: expose Peanut01 actuation state and control"
```

### Task 3: Add Explicit Cross-Machine Transport

**Files:**
- Create: `src/tod_network/tod_communication_interface/src/operator/actuation_control_service_forwarder.cpp`
- Create: `src/tod_network/tod_communication_interface/src/vehicle/actuation_control_service_listener.cpp`
- Modify: `src/tod_network/tod_communication_interface/CMakeLists.txt`
- Modify: `src/tod_network/tod_communication_interface/package.xml`
- Modify: `src/tod_network/tod_communication_interface/src/operator/CMakeLists.txt`
- Modify: `src/tod_network/tod_communication_interface/src/vehicle/CMakeLists.txt`
- Modify: `src/tod_network/tod_communication_interface/launch/tod_communication_interface_operator.launch.py`
- Modify: `src/tod_network/tod_communication_interface/launch/tod_communication_interface_vehicle.launch.py`
- Modify: `config/config/package_config/tod_communication_interface/services.yaml`
- Modify: `config/config/package_config/tod_data_interface/operator_config.yaml`
- Modify: `config/config/package_config/tod_data_interface/vehicle_config.yaml`
- Modify: `config/config/remappings.yaml`
- Test: `tests/test_visual_actuation_control.py`

- [ ] **Step 1: Write failing transport contract tests**

Assert the service is configured as TCP on 60500/60501, both launch files
recognize `ActuationControlService`, both C++ nodes instantiate
`ServiceForwarder`/`ServiceListener<tod_vehicle_msgs::srv::SetActuationEnabled>`,
and both CMake target lists depend on `tod_vehicle_msgs`. Assert UDP port 60012
is a `send_always` state sender and matching receiver. Assert remappings connect
the bridge service/state to transport and Visual.

- [ ] **Step 2: Run transport tests and verify RED**

Run: `python -m pytest tests/test_visual_actuation_control.py -q`

Expected: FAIL on missing forwarding targets and config entries.

- [ ] **Step 3: Add forwarding executables**

Follow the existing network-monitor pair exactly, substituting:

```cpp
#include <tod_vehicle_msgs/srv/set_actuation_enabled.hpp>
using Service = tod_vehicle_msgs::srv::SetActuationEnabled;
```

Create executable names `ActuationControlServiceForwarder` and
`ActuationControlServiceListener`, and add `tod_vehicle_msgs` to package and
CMake dependencies.

- [ ] **Step 4: Wire service and state transport**

Add this service entry:

```yaml
ActuationControlService:
  protocol: TCP
  service_vehicle: from_operator/set_actuation_enabled
  service_operator: to_vehicle/set_actuation_enabled
  forwarder_port: 60500
  listener_port: 60501
```

Add `ActuationControlStateSender` and `ActuationControlStateReceiver` on UDP
port 60012 with type `tod_vehicle_msgs/msg/ActuationControlState`; configure the
sender with `send_always: True` and control modes including 99. Add remappings:

```yaml
/vehicle/network/config/from_operator/set_actuation_enabled
  -> /vehicle/interface/peanut01/ControlBridge/set_actuation_enabled
/vehicle/network/data/to_operator/actuation_control_state
  -> /vehicle/interface/peanut01/actuation_control_state
/operator/interface/visual/input/actuation_control_state
  -> /operator/network/data/from_vehicle/actuation_control_state
```

- [ ] **Step 5: Run transport tests and verify GREEN**

Run: `python -m pytest tests/test_visual_actuation_control.py tests/test_deployment_config.py -q`

Expected: PASS.

- [ ] **Step 6: Commit transport**

```bash
git add src/tod_network/tod_communication_interface config/config \
  tests/test_visual_actuation_control.py
git commit -m "feat: transport real-actuation control and state"
```

### Task 4: Add Focused Visual ROS Components

**Files:**
- Create: `src/tod_operator_interface/tod_visual/src/tod_gl/include/tod_gl/ros_interface/subscribing_components/actuation_control_state_component.hpp`
- Create: `src/tod_operator_interface/tod_visual/src/tod_gl/src/ros_interface/subscribing_components/actuation_control_state_component.cpp`
- Create: `src/tod_operator_interface/tod_visual/src/tod_gl/include/tod_gl/ros_interface/service_components/actuation_control_component.hpp`
- Create: `src/tod_operator_interface/tod_visual/src/tod_gl/src/ros_interface/service_components/actuation_control_component.cpp`
- Modify: `src/tod_operator_interface/tod_visual/src/tod_gl/CMakeLists.txt`
- Modify: `src/tod_operator_interface/tod_visual/src/tod_applications/visual/application_layer/src/visual_io_layer.cpp`
- Test: `tests/test_visual_actuation_control.py`

- [ ] **Step 1: Write failing component tests**

Assert the subscriber owns state, reason, requested-enable, and steady-clock
receipt time behind a mutex; exposes a snapshot and one-second freshness test;
and subscribes to `input/actuation_control_state`. Assert the client calls the
absolute forwarded service, rejects duplicate requests, records a two-second
timeout, and exposes pending/error state.

- [ ] **Step 2: Run component tests and verify RED**

Run: `python -m pytest tests/test_visual_actuation_control.py -q`

Expected: FAIL because both components are absent.

- [ ] **Step 3: Implement the state component**

Use a value snapshot rather than returning references across the ROS/UI threads:

```cpp
struct ActuationControlSnapshot {
  uint8_t state;
  bool enable_requested;
  std::string reason;
  std::chrono::steady_clock::time_point received_at;
  bool received;
};
```

The callback replaces the snapshot under `std::mutex`. `is_fresh(1s)` compares
the receipt time to `steady_clock::now()`, not ROS time.

- [ ] **Step 4: Implement the async service component**

Create a client for
`/operator/network/config/to_vehicle/set_actuation_enabled`. `request(bool)`
returns false when pending or unavailable; otherwise sets pending and sends one
async request. `update()` marks requests older than two seconds as timed out.
The response callback stores rejection reasons but never changes the confirmed
vehicle state.

- [ ] **Step 5: Register and test components**

Add both source files to `TOD_GL_SOURCES` and add
`ActuationControlStateComponent` to the Visual `SubscriptionManager`.

Run: `python -m pytest tests/test_visual_actuation_control.py -q`

Expected: PASS.

- [ ] **Step 6: Commit Visual ROS ownership**

```bash
git add src/tod_operator_interface/tod_visual/src/tod_gl \
  src/tod_operator_interface/tod_visual/src/tod_applications/visual/application_layer/src/visual_io_layer.cpp \
  tests/test_visual_actuation_control.py
git commit -m "feat: add Visual actuation ROS components"
```

### Task 5: Render And Operate The Lower-Left Control

**Files:**
- Modify: `src/tod_operator_interface/tod_visual/src/tod_applications/visual/application_layer/include/drive_info_layer.hpp`
- Modify: `src/tod_operator_interface/tod_visual/src/tod_applications/visual/visual_node.cpp`
- Test: `tests/test_visual_actuation_control.py`

- [ ] **Step 1: Write failing UI behavior tests**

Assert the layer uses a fixed button dimension and lower-left position, defines
a two-second hold, draws a progress arc and familiar power symbol, maps the four
states to gray/amber/blue/red, uses `is_fresh`, displays `NO DATA`, and invokes
`request(false)` on click for `ARMING/ACTIVE` while invoking `request(true)`
only after the hold threshold for `DISABLED`. Assert `FAULT` hold invokes
`request(false)` only.

- [ ] **Step 2: Run UI tests and verify RED**

Run: `python -m pytest tests/test_visual_actuation_control.py -q`

Expected: FAIL because the layer has no actuation control.

- [ ] **Step 3: Integrate state and service ownership**

Add `ActuationControlStateComponent` as a `DriveInfoLayer` template parameter
and instantiate `ActuationControlComponent` from the layer's ROS node. Pull a
fresh snapshot during `on_update`; never infer a state from a service response.

- [ ] **Step 4: Render fixed lower-left button**

Use `ImGui::InvisibleButton` with a stable 48x48 size at a 24-pixel left margin.
Draw the standard power symbol with `ImDrawList::PathArcTo` plus a vertical line,
so no unsuitable existing bitmap or font-dependent glyph is required. Draw the
two-second hold progress as an outer arc. Place compact state text and fault or
request-error reason to the right without resizing the button.

- [ ] **Step 5: Implement state-specific input**

Use `ImGui::GetIO().MouseDownDuration[0]` only while the power button is active.
At 2.0 seconds, send one request and latch the hold until mouse release.
`DISABLED` sends true; `FAULT` sends false. A click in `ARMING` or `ACTIVE`
sends false immediately. Stale/no feedback and pending requests accept no input.

- [ ] **Step 6: Run UI and regression tests**

Run: `python -m pytest tests/test_visual_actuation_control.py tests/test_operator_latency_monitoring.py -q`

Expected: PASS, including the RTT-based network bar contract.

- [ ] **Step 7: Commit the Visual interaction**

```bash
git add src/tod_operator_interface/tod_visual/src/tod_applications/visual \
  tests/test_visual_actuation_control.py
git commit -m "feat: add Visual real-actuation power control"
```

### Task 6: Full Verification And Remote Deployment

**Files:**
- Verify all files changed in Tasks 1-5.

- [ ] **Step 1: Run the complete local test suite**

Run: `python -m pytest -q`

Expected: all tests and subtests pass.

- [ ] **Step 2: Check patch hygiene**

Run: `git diff --check`

Expected: no whitespace errors.

- [ ] **Step 3: Build shared, vehicle, transport, and Visual packages**

In the existing image-based build environment, build at minimum:

```bash
colcon build --packages-up-to tod_vehicle_msgs tod_communication_interface tod_visual
```

Expected: all selected packages finish successfully.

- [ ] **Step 4: Build rollback-tagged operator and vehicle images**

Preserve the currently deployed image IDs under dated rollback tags. Build new
images containing generated interfaces, communication executables, bridge
script, configs, and Visual binary. Do not set `enable_actuation=true` in any
image or mounted YAML.

- [ ] **Step 5: Deploy vehicle first and verify disabled state**

Recreate only `tod_vehicle_edge`. Verify:

```text
/vehicle/interface/peanut01/ControlBridge/set_actuation_enabled exists
/vehicle/interface/peanut01/actuation_control_state publishes DISABLED
real control topic publisher counts remain zero
```

Do not perform physical movement during deployment verification.

- [ ] **Step 6: Deploy operator and verify end to end**

Recreate only `tod_operator_edge` with all current compose override files,
including the local G923 mapping. Verify the forwarded service exists, the
operator state topic has one publisher and Visual subscriber, and the UI starts
at `DISABLED` or `NO DATA`, never `ACTIVE`.

- [ ] **Step 7: Verify safe transitions without motion**

With neutral, zero requested velocity, and the vehicle stationary, hold enable
and observe `DISABLED -> ARMING`. If all existing gates permit `ACTIVE`, click
stop immediately and verify `DISABLED`; otherwise record the returned
`ARMING/FAULT` reason. Do not command nonzero velocity or change gear as part of
this verification.

- [ ] **Step 8: Final regression evidence**

Record local test totals, built image IDs, rollback tags, container status,
service/topic endpoint counts, and the final confirmed vehicle actuation state.

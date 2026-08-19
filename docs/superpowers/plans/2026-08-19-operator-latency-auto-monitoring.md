# Operator Latency Auto-Monitoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Automatically start both operator-side and vehicle-side latency monitoring when Visual connects, and stop both when it disconnects.

**Architecture:** Extend `NetworkMonitorComponent` from one forwarded service client to two independent clients: a local operator monitor client and the existing vehicle-forwarding client. Manager passes the selected operator IP and entered vehicle IP on connection transitions; the existing local metrics topic continues to feed Visual.

**Tech Stack:** C++17, ROS 2 Humble services, rclcpp, pytest contract tests, Docker Compose

---

### Task 1: Define the Dual-Monitor Contract

**Files:**
- Create: `tests/test_operator_latency_monitoring.py`
- Inspect: `config/config/remappings.yaml`
- Inspect: `src/tod_operator_interface/tod_visual/src/tod_gl/include/tod_gl/ros_interface/service_components/network_monitor_component.hpp`
- Inspect: `src/tod_operator_interface/tod_visual/src/tod_gl/src/ros_interface/service_components/network_monitor_component.cpp`
- Inspect: `src/tod_operator_interface/tod_visual/src/tod_applications/manager/src/application_layer/src/operator_state_layer.cpp`

- [ ] **Step 1: Write the failing contract tests**

```python
import pathlib

import yaml


ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / (
    "src/tod_operator_interface/tod_visual/src/tod_gl/include/tod_gl/"
    "ros_interface/service_components/network_monitor_component.hpp"
)
COMPONENT = ROOT / (
    "src/tod_operator_interface/tod_visual/src/tod_gl/src/ros_interface/"
    "service_components/network_monitor_component.cpp"
)
STATE_LAYER = ROOT / (
    "src/tod_operator_interface/tod_visual/src/tod_applications/manager/"
    "src/application_layer/src/operator_state_layer.cpp"
)
REMAPPINGS = ROOT / "config/config/remappings.yaml"


def test_network_monitor_component_owns_local_and_vehicle_clients():
    header = HEADER.read_text(encoding="utf-8")
    source = COMPONENT.read_text(encoding="utf-8")

    assert "local_client_" in header
    assert "vehicle_client_" in header
    assert (
        '"/operator/monitoring/network_monitor/set_monitoring_status"'
        in source
    )
    assert "to_vehicle/set_monitoring_status" in source


def test_manager_starts_and_stops_both_monitors_with_directional_ips():
    state_layer = STATE_LAYER.read_text(encoding="utf-8")

    assert (
        "_networkMonitor.SetMonitorStatus("
        "ipOperatorOptions[selectedOperatorIP], inputBuffer, true);"
        in state_layer
    )
    assert (
        "_networkMonitor.SetMonitorStatus("
        "ipOperatorOptions[selectedOperatorIP], inputBuffer, false);"
        in state_layer
    )


def test_visual_keeps_using_local_operator_metrics():
    remappings = yaml.safe_load(REMAPPINGS.read_text(encoding="utf-8"))
    pairs = {
        (entry["from"], entry["to"])
        for entry in remappings["tod_visual"]
    }

    assert (
        "/operator/interface/visual/input/network_metrics",
        "/operator/monitoring/output/network_metrics",
    ) in pairs
```

- [ ] **Step 2: Run the focused tests and verify RED**

Run:

```bash
pytest -q tests/test_operator_latency_monitoring.py
```

Expected: two failures because `local_client_`, `vehicle_client_`, and the three-argument Manager calls do not exist; the remapping test passes.

- [ ] **Step 3: Commit the failing contract**

```bash
git add tests/test_operator_latency_monitoring.py
git commit -m "test: define automatic latency monitoring contract"
```

### Task 2: Start Both Monitors on Connection

**Files:**
- Modify: `src/tod_operator_interface/tod_visual/src/tod_gl/include/tod_gl/ros_interface/service_components/network_monitor_component.hpp`
- Modify: `src/tod_operator_interface/tod_visual/src/tod_gl/src/ros_interface/service_components/network_monitor_component.cpp`
- Modify: `src/tod_operator_interface/tod_visual/src/tod_applications/manager/src/application_layer/src/operator_state_layer.cpp`
- Test: `tests/test_operator_latency_monitoring.py`

- [ ] **Step 1: Change the component interface and client ownership**

Replace the public operation and private members with:

```cpp
void SetMonitorStatus(
    const std::string &operator_ip,
    const std::string &vehicle_ip,
    bool set_active);

private:
using MonitorService = tod_network_monitoring_msgs::srv::NetworkMonitorService;
using MonitorClient = rclcpp::Client<MonitorService>;

void SendRequest(
    const MonitorClient::SharedPtr &client,
    const std::string &target_ip,
    bool set_active,
    const std::string &endpoint_name);

std::shared_ptr<rclcpp::Node> node_;
MonitorClient::SharedPtr local_client_;
MonitorClient::SharedPtr vehicle_client_;
```

- [ ] **Step 2: Create both clients in the constructor**

Create the local client with the stable absolute service name:

```cpp
local_client_ = node_->create_client<MonitorService>(
    "/operator/monitoring/network_monitor/set_monitoring_status");
```

Keep discovery of the service containing `to_vehicle/set_monitoring_status`, then create `vehicle_client_` from the discovered absolute name. Log a warning for either unavailable client, but do not return early and prevent the other client from working.

- [ ] **Step 3: Implement independent asynchronous requests**

Implement `SendRequest` so it checks `service_is_ready()`, logs the endpoint name and target IP when unavailable, sends the request asynchronously when ready, and logs a negative response or exception without changing TOD connection state.

Implement the public operation as:

```cpp
void NetworkMonitorComponent::SetMonitorStatus(
    const std::string &operator_ip,
    const std::string &vehicle_ip,
    bool set_active) {
    SendRequest(local_client_, vehicle_ip, set_active, "operator");
    SendRequest(vehicle_client_, operator_ip, set_active, "vehicle");
}
```

- [ ] **Step 4: Pass both directional IPs from Manager**

Change the `CONNECTED` transition to:

```cpp
_networkMonitor.SetMonitorStatus(
    ipOperatorOptions[selectedOperatorIP], inputBuffer, true);
```

Change the `DISCONNECTED` transition to:

```cpp
_networkMonitor.SetMonitorStatus(
    ipOperatorOptions[selectedOperatorIP], inputBuffer, false);
```

- [ ] **Step 5: Run the focused tests and verify GREEN**

Run:

```bash
pytest -q tests/test_operator_latency_monitoring.py
```

Expected: `3 passed`.

- [ ] **Step 6: Run the complete contract suite**

Run:

```bash
pytest -q tests
```

Expected: all tests pass with no regressions.

- [ ] **Step 7: Check patch formatting and commit**

```bash
git diff --check
git add \
  src/tod_operator_interface/tod_visual/src/tod_gl/include/tod_gl/ros_interface/service_components/network_monitor_component.hpp \
  src/tod_operator_interface/tod_visual/src/tod_gl/src/ros_interface/service_components/network_monitor_component.cpp \
  src/tod_operator_interface/tod_visual/src/tod_applications/manager/src/application_layer/src/operator_state_layer.cpp
git commit -m "fix: start latency monitoring with operator connection"
```

### Task 3: Build, Deploy, and Verify the Operator

**Files:**
- Deploy repository: `/home/wufan/teleoperated_driving` on `wufan@192.168.68.69`
- Compose files: `docker-compose.yaml`, `docker-compose.override.yaml`, `docker-compose.peanut01-video.yaml`

- [ ] **Step 1: Record the live operator image and configuration**

Run remotely:

```bash
docker inspect tod_operator_edge --format '{{.Image}}'
docker inspect tod_operator_edge --format '{{range .Config.Env}}{{println .}}{{end}}' \
  | grep -E 'ROS_DOMAIN_ID|CYCLONEDDS_URI|DISPLAY'
```

Expected: the current image ID is recorded and `ROS_DOMAIN_ID=7` remains configured.

- [ ] **Step 2: Preserve a rollback image tag**

Run remotely:

```bash
operator_latency_image_id=$(docker inspect tod_operator_edge --format '{{.Image}}')
docker tag "$operator_latency_image_id" \
  ghcr.io/sl-kai/teleoperated-driving-operator:rollback-pre-auto-latency-20260819
docker image inspect \
  ghcr.io/sl-kai/teleoperated-driving-operator:rollback-pre-auto-latency-20260819 \
  --format '{{.Id}}'
```

Expected: `docker image inspect` succeeds for the rollback tag.

- [ ] **Step 3: Transfer the committed source without changing unrelated remote configuration**

Create the bundle locally:

```bash
git bundle create operator-auto-latency.bundle ros2
```

Transfer it to `/tmp/operator-auto-latency.bundle` on `wufan@192.168.68.69`, then run remotely:

```bash
cd /home/wufan/teleoperated_driving
git fetch /tmp/operator-auto-latency.bundle ros2
git merge --ff-only FETCH_HEAD
rm -f /tmp/operator-auto-latency.bundle
```

Remove the local `operator-auto-latency.bundle` after the fetch succeeds. Preserve the remote `.env` and mounted `config/config` values.

Verify:

```bash
git status --short
git log -3 --oneline
```

Expected: only the pre-existing deployment configuration changes remain, and the automatic latency implementation commit is present.

- [ ] **Step 4: Build the operator image from the updated source**

Run remotely from `/home/wufan/teleoperated_driving`:

```bash
DOCKER_BUILDKIT=0 docker compose \
  -f docker-compose.yaml \
  -f docker-compose.override.yaml \
  -f docker-compose.peanut01-video.yaml \
  build tod_operator
```

Expected: the `tod_operator` target completes and contains the rebuilt `tod_visual` package. If the host cannot access Docker Hub, reuse the already cached base stages and do not pull or replace the running container until the build succeeds.

- [ ] **Step 5: Recreate only the operator service**

Run remotely:

```bash
docker compose \
  -f docker-compose.yaml \
  -f docker-compose.override.yaml \
  -f docker-compose.peanut01-video.yaml \
  up -d --no-deps --force-recreate tod_operator
```

Expected: `tod_operator_edge` is running; no vehicle container is touched.

- [ ] **Step 6: Verify the ROS topology before connection**

Run in `tod_operator_edge` after sourcing ROS:

```bash
ros2 param get /operator/monitoring/network_monitor network_interface
ros2 topic info -v /operator/monitoring/output/network_metrics
```

Expected: interface is `wlp5s0`, one publisher is `network_monitor`, and one subscriber is `Visual`.

- [ ] **Step 7: Verify automatic start after connecting to the vehicle**

Connect Visual to vehicle IP `192.168.68.32`, then run:

```bash
timeout 10 ros2 topic echo \
  /operator/monitoring/output/network_metrics \
  tod_network_monitoring_msgs/msg/NetworkMetrics --once
```

Expected: a message arrives without any manual service call and `latency` is greater than zero while the vehicle responds to ICMP.

- [ ] **Step 8: Verify lifecycle behavior**

Disconnect in Visual and confirm the topic stops publishing within two seconds. Reconnect and confirm a new metrics message arrives within ten seconds. Review container logs for endpoint-specific monitoring errors.

- [ ] **Step 9: Final verification**

Run locally:

```bash
pytest -q tests
git diff --check
git status --short
```

Expected: all tests pass, formatting is clean, and only the user's pre-existing untracked files remain.

# Peanut01 1000 ms Command Timeout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persistently increase only the Peanut01 TOD command freshness timeout from 300 ms to 1000 ms and deploy the change with actuation disabled.

**Architecture:** Keep the YAML, ControlBridge fallback, and supervisor fallback synchronized at 1000 ms while retaining all physical-feedback timeouts and fault-latching behavior. Build a small offline overlay from the currently deployed integrated vehicle image because the vehicle cannot reach Docker Hub, then recreate only `tod_vehicle`.

**Tech Stack:** Python 3, pytest, ROS 2 Humble parameters, Docker Compose, Git bundle, Bash.

---

### Task 1: Define The One-Second Command Timeout Contract

**Files:**
- Modify: `tests/test_peanut01_control_bridge.py`
- Read: `config/config/package_config/tod_peanut01_interface/params.yaml`
- Read: `work/peanut01_control_bridge.py`
- Read: `work/peanut01_control_supervisor.py`

- [ ] **Step 1: Add a failing consistency test**

Add a test that reads the YAML and both Python sources and requires every
command timeout default to be one second:

```python
SUPERVISOR = ROOT / "work/peanut01_control_supervisor.py"


def test_command_timeout_defaults_to_one_second_everywhere(self):
    params = yaml.safe_load(PARAMS.read_text(encoding="utf-8"))
    shared = params["/**"]["ros__parameters"]
    node = params[
        "/vehicle/interface/peanut01/ControlBridge"
    ]["ros__parameters"]
    bridge = BRIDGE.read_text(encoding="utf-8")
    supervisor = SUPERVISOR.read_text(encoding="utf-8")

    self.assertEqual(1000, shared["command_timeout_ms"])
    self.assertEqual(1000, node["command_timeout_ms"])
    self.assertIn(
        'declare_parameter("command_timeout_ms", 1000)', bridge
    )
    self.assertIn(
        "command_timeout_ns: int = 1_000_000_000", supervisor
    )
```

Also change the existing shared-parameter expectation from 300 to 1000.

- [ ] **Step 2: Verify the new contract fails for the old defaults**

Run:

```powershell
python -m pytest tests/test_peanut01_control_bridge.py -v
```

Expected: the new consistency test and updated YAML expectation fail because
the current value is 300 ms; unrelated control tests pass.

- [ ] **Step 3: Commit the red contract**

```powershell
git add tests/test_peanut01_control_bridge.py
git commit -m "test: require one-second TOD command timeout"
```

### Task 2: Synchronize Persistent Timeout Defaults

**Files:**
- Modify: `config/config/package_config/tod_peanut01_interface/params.yaml`
- Modify: `work/peanut01_control_bridge.py`
- Modify: `work/peanut01_control_supervisor.py`
- Test: `tests/test_peanut01_control_bridge.py`

- [ ] **Step 1: Update both YAML command timeout entries**

Set only these values to 1000:

```yaml
/**:
  ros__parameters:
    command_timeout_ms: 1000

/vehicle/interface/peanut01/ControlBridge:
  ros__parameters:
    command_timeout_ms: 1000
```

Leave `feedback_timeout_ms` and `can_feedback_timeout_ms` at 300.

- [ ] **Step 2: Update the ControlBridge fallback**

Change the fallback declaration to:

```python
source_node.declare_parameter("command_timeout_ms", 1000).value
```

- [ ] **Step 3: Update the supervisor fallback**

Change the dataclass default to:

```python
command_timeout_ns: int = 1_000_000_000
```

- [ ] **Step 4: Run focused tests**

```powershell
python -m pytest tests/test_peanut01_control_bridge.py tests/test_peanut01_can_feedback.py -v
```

Expected: all focused tests and subtests pass.

- [ ] **Step 5: Run full local verification**

```powershell
python -m pytest -q
$env:XAUTHORITY='/tmp/.Xauthority'
$env:DISPLAY=':0'
$env:XDG_RUNTIME_DIR='/tmp/runtime'
docker compose config --quiet
git diff --check
```

Expected: the full suite passes and both Compose and whitespace checks exit
zero.

- [ ] **Step 6: Commit the implementation**

```powershell
git add config/config/package_config/tod_peanut01_interface/params.yaml `
  work/peanut01_control_bridge.py work/peanut01_control_supervisor.py
git commit -m "fix: allow one-second TOD command gap"
```

### Task 3: Deploy And Verify Safely On The Vehicle

**Files:**
- Deploy to: `aiec@192.168.68.32:/userdata/teleoperated_driving`
- Runtime config: `/userdata/teleoperated_driving/.env`
- Runtime container: `tod_vehicle_edge`

- [ ] **Step 1: Record and enforce the safe precondition**

Read `enable_actuation`. If it is true, set it false and verify the resulting
value is false. Record the states of `minguo`, `peanut01-drivers`, and
`algo_dev_arm_0813`; do not modify or restart them.

- [ ] **Step 2: Transfer the committed range and fast-forward the remote branch**

Create a prerequisite bundle containing commits after the currently deployed
`0e7f607`, transfer it to `/home/aiec`, fetch it, and fast-forward remote
`ros2`. Preserve the vehicle-specific `.env`, including
`VEHICLE_NETWORK_INTERFACE=wlan0`.

- [ ] **Step 3: Build an offline vehicle overlay**

Build a new image from the current local
`ghcr.io/sl-kai/teleoperated-driving-vehicle:edge` image with these files
overlaid:

```dockerfile
FROM ghcr.io/sl-kai/teleoperated-driving-vehicle:edge
USER root
COPY work/peanut01_control_bridge.py /opt/tod-tools/peanut01_control_bridge.py
COPY work/peanut01_control_supervisor.py /opt/tod-tools/peanut01_control_supervisor.py
RUN grep -q 'declare_parameter("command_timeout_ms", 1000)' /opt/tod-tools/peanut01_control_bridge.py
RUN grep -q 'command_timeout_ns: int = 1_000_000_000' /opt/tod-tools/peanut01_control_supervisor.py
USER tum
```

Use `DOCKER_BUILDKIT=0` to avoid external metadata resolution. Tag the previous
edge image as `rollback-pre-command-timeout-1000ms-20260819` before assigning
the new image to the `edge` tag.

- [ ] **Step 4: Recreate only the vehicle service**

```bash
docker compose up -d --no-deps --force-recreate --no-build --pull never tod_vehicle
```

- [ ] **Step 5: Verify runtime parameters and safety topology**

Confirm:

```text
command_timeout_ms = 1000
feedback_timeout_ms = 300
can_feedback_timeout_ms = 300
execution_confirmation_timeout_ms = 1000
enable_actuation = false
```

Also confirm one integrated relay process, one `/vehicle/can/raw` publisher,
one `peanut01_control_target` subscriber, zero real-control publishers, and no
standalone relay sidecar.

- [ ] **Step 6: Verify protected containers and clean temporary transfer files**

Confirm `minguo`, `peanut01-drivers`, and `algo_dev_arm_0813` retained their
running state. Remove only the explicitly named local and remote temporary Git
bundle. Keep the source backup branch and rollback image.

# Integrated CAN Feedback Relay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the read-only CAN feedback relay as an automatically respawned ROS 2 process inside the single `tod_vehicle_edge` container and remove the relay sidecar deployment.

**Architecture:** The existing C++ relay remains a separate process launched by the Peanut01 vehicle launch with ROS Domain 0 and a one-second respawn delay. The vehicle image contains the relay executable, while ControlBridge retains all existing feedback timeouts and fail-closed behavior.

**Tech Stack:** ROS 2 Humble launch, C++17/rclcpp, Python launch files, Docker multi-stage builds, Docker Compose, pytest.

---

### Task 1: Add Failing Integration Contract Tests

**Files:**
- Modify: `tests/test_can_feedback_relay_contract.py`
- Read: `src/tod_launch/launch/tod_vehicle_peanut01_video.launch.py`
- Read: `docker/dockerfile`
- Read: `docker-compose.yaml`

- [ ] **Step 1: Add launch, image, and Compose contract tests**

Add these paths and tests to `tests/test_can_feedback_relay_contract.py`:

```python
LAUNCH = ROOT / "src/tod_launch/launch/tod_vehicle_peanut01_video.launch.py"
DOCKERFILE = ROOT / "docker/dockerfile"
COMPOSE = ROOT / "docker-compose.yaml"


def test_vehicle_launch_runs_relay_in_domain_zero_with_respawn():
    text = LAUNCH.read_text(encoding="utf-8")

    assert 'package="tod_can_feedback_relay"' in text
    assert 'executable="tod_can_feedback_relay"' in text
    assert '"interface": "can0"' in text
    assert 'additional_env={"ROS_DOMAIN_ID": "0"}' in text
    assert "respawn=True" in text
    assert "respawn_delay=1.0" in text


def test_vehicle_image_requires_installed_relay_executable():
    text = DOCKERFILE.read_text(encoding="utf-8")

    assert (
        "install/tod_can_feedback_relay/lib/tod_can_feedback_relay/"
        "tod_can_feedback_relay"
    ) in text


def test_compose_has_no_relay_sidecar():
    text = COMPOSE.read_text(encoding="utf-8")

    assert "tod_can_feedback_relay:" not in text
    assert "TOD_CAN_FEEDBACK_RELAY_IMAGE" not in text
```

- [ ] **Step 2: Run the new tests and verify they fail for the missing integration**

Run:

```bash
python -m pytest tests/test_can_feedback_relay_contract.py -v
```

Expected: the two existing relay implementation tests pass and the three new integration tests fail because the launch action, image assertion, and sidecar removal do not exist yet.

- [ ] **Step 3: Commit the red tests**

```bash
git add tests/test_can_feedback_relay_contract.py
git commit -m "test: define integrated CAN relay contract"
```

### Task 2: Integrate Relay Into Vehicle Launch And Image

**Files:**
- Modify: `src/tod_launch/launch/tod_vehicle_peanut01_video.launch.py`
- Modify: `docker/dockerfile`
- Modify: `docker-compose.yaml`
- Test: `tests/test_can_feedback_relay_contract.py`

- [ ] **Step 1: Add the respawned relay process to the Peanut01 launch**

Before returning the launch description, add:

```python
    description.add_action(
        Node(
            package="tod_can_feedback_relay",
            executable="tod_can_feedback_relay",
            name="tod_can_feedback_relay",
            output="screen",
            parameters=[{"interface": "can0"}],
            additional_env={"ROS_DOMAIN_ID": "0"},
            respawn=True,
            respawn_delay=1.0,
        )
    )
```

This creates a separate child process without changing the ROS domain of the rest of the vehicle launch.

- [ ] **Step 2: Fail the final image build if the relay executable is absent**

Immediately after copying the builder install tree into the `tod_vehicle` stage, add:

```dockerfile
RUN test -x /home/${DOCKER_USERNAME}/wsp/install/tod_can_feedback_relay/lib/tod_can_feedback_relay/tod_can_feedback_relay
```

- [ ] **Step 3: Remove the relay sidecar service**

Delete the complete `tod_can_feedback_relay:` service block from `docker-compose.yaml`, including its local image variable, build configuration, capabilities, environment, volumes, and command.

- [ ] **Step 4: Run focused tests**

Run:

```bash
python -m pytest tests/test_can_feedback_relay_contract.py tests/test_peanut01_control_bridge.py -v
```

Expected: all focused tests pass.

- [ ] **Step 5: Run the full repository test suite**

Run:

```bash
python -m pytest -q
```

Expected: no failures, including all existing subtests.

- [ ] **Step 6: Validate Compose rendering and whitespace**

Run:

```bash
docker compose config --quiet
git diff --check
```

Expected: both commands exit zero.

- [ ] **Step 7: Commit the implementation**

```bash
git add src/tod_launch/launch/tod_vehicle_peanut01_video.launch.py \
  docker/dockerfile docker-compose.yaml
git commit -m "feat: run CAN feedback relay in vehicle container"
```

### Task 3: Build, Deploy, And Verify On The Vehicle

**Files:**
- Deploy source tree to: `aiec@192.168.68.32:/userdata/teleoperated_driving`
- Build target: `tod_vehicle`
- Runtime container: `tod_vehicle_edge`

- [ ] **Step 1: Record the current safe state**

Read the current diagnostics and set `enable_actuation=false` if it is not already false. Do not issue motion commands. Verify `minguo` and `peanut01-drivers` are running and do not restart or modify them.

- [ ] **Step 2: Transfer the committed source without overwriting unrelated remote files**

Transfer the new commits to `/userdata/teleoperated_driving`, update the checked-out branch, and verify:

```bash
git log -3 --oneline
git status --short
```

Expected: the integrated-relay implementation commit is present; unrelated remote changes remain preserved.

- [ ] **Step 3: Build the integrated vehicle image**

From `/userdata/teleoperated_driving`, run the existing vehicle build path for `tod_vehicle`. Verify inside the resulting image:

```bash
test -x /home/tum/wsp/install/tod_can_feedback_relay/lib/tod_can_feedback_relay/tod_can_feedback_relay
```

Expected: the image build succeeds and the executable check exits zero.

- [ ] **Step 4: Stop only the obsolete sidecar before migration**

Run:

```bash
docker stop tod_can_feedback_relay_edge
docker rm tod_can_feedback_relay_edge
```

If the old sidecar does not exist, record that fact and continue. Do not remove
the rollback image and do not change `minguo`, `peanut01-drivers`, or other
system Docker containers.

- [ ] **Step 5: Recreate only the vehicle container**

Run the repository's deployment command equivalent to:

```bash
docker compose up -d --no-deps --force-recreate tod_vehicle
```

Expected: `tod_vehicle_edge` starts from the newly built integrated image.

- [ ] **Step 6: Verify the single-container topology**

Run:

```bash
docker ps --format '{{.Names}}' | grep '^tod_vehicle_edge$'
docker ps --format '{{.Names}}' | grep 'tod_can_feedback_relay' && exit 1 || true
docker exec tod_vehicle_edge pgrep -af tod_can_feedback_relay
```

Expected: one vehicle container, no relay sidecar, and one relay child process.

- [ ] **Step 7: Verify ROS and CAN feedback while actuation remains disabled**

In ROS Domain 0, verify `/vehicle/can/raw` has one publisher and `peanut01_control_target` has one subscription. Sample diagnostics and confirm fresh MCU/EPS ages, an empty CAN rejection reason, and no real-control publishers.

- [ ] **Step 8: Verify child-process respawn safely**

With `enable_actuation=false`, record the relay PID, terminate only that child process, and poll for a different PID. Confirm `tod_vehicle_edge` stays running, the relay returns in about one second, and `/vehicle/can/raw` returns to one publisher.

- [ ] **Step 9: Run a final read-only health snapshot**

Confirm the relay, ControlBridge, CAN feedback, and vehicle container are healthy. Report that no physical movement test was performed and leave `enable_actuation=false`.

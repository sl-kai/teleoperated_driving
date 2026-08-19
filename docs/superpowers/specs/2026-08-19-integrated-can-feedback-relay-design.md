# Integrated CAN Feedback Relay Design

## Goal

Ship and run the read-only `tod_can_feedback_relay` as part of the existing
vehicle image and `tod_vehicle_edge` container. The deployment must no longer
require a relay-specific image or Compose sidecar.

## Selected Architecture

The existing C++ relay remains an independent ROS 2 process, but the Peanut01
vehicle launch starts and supervises it inside `tod_vehicle_edge`.

The relay process uses:

- ROS domain 0;
- SocketCAN interface `can0`;
- topic `/vehicle/can/raw` with type `std_msgs/msg/String`;
- the existing read-only CAN ID filter;
- launch-level process respawning with a one-second delay.

The vehicle container continues to use host networking and the existing
`NET_RAW` capability. No CAN transmission behavior is added.

## Data Flow

```text
host can0
  -> tod_can_feedback_relay (Domain 0, child process)
  -> /vehicle/can/raw
  -> peanut01_control_target
  -> ControlBridge safety supervisor
```

The main vehicle launch still runs in its configured ROS domain. The relay is
given `ROS_DOMAIN_ID=0` explicitly so that its output reaches the Domain 0
control target without changing the domain of other vehicle nodes.

## Image And Launch Changes

The vehicle builder compiles `tod_can_feedback_relay` with the rest of the
workspace, and the final `tod_vehicle` stage copies the installed executable.
The image build must fail if the executable is missing.

`tod_vehicle_peanut01_video.launch.py` starts the relay as an independent ROS 2
process with:

- `interface:=can0`;
- `ROS_DOMAIN_ID=0` in the child environment;
- `respawn=True`;
- `respawn_delay=1.0`.

The independent `tod_can_feedback_relay` Compose service is removed. A normal
vehicle deployment therefore creates only `tod_vehicle_edge` for TOD vehicle
software and exactly one `/vehicle/can/raw` publisher.

## Failure Handling

If the relay cannot open `can0` or exits unexpectedly, ROS launch restarts only
the relay process after one second. The rest of `tod_vehicle_edge` remains
running.

The existing 300 ms feedback timeout remains unchanged. If relay feedback is
interrupted, ControlBridge fails closed, enters or remains in its latched fault
state, and destroys the real-control publishers. Relay recovery restores CAN
feedback only. It must not automatically clear the fault or re-enable physical
control. An operator must set `enable_actuation=false`, confirm safe conditions,
and explicitly rearm.

The design does not bypass emergency, approval, neutral, MCU, execution
confirmation, or command freshness checks.

## Deployment Migration

Before recreating the integrated vehicle container, the old relay sidecar must
be stopped. This prevents two processes from reading `can0` and publishing the
same ROS topic during migration.

The local `tod-can-feedback-relay:local` image may remain temporarily for
rollback, but deployment configuration no longer references or starts it.

## Verification

Automated checks must verify:

1. The vehicle launch includes the relay with Domain 0, `can0`, respawn enabled,
   and a one-second respawn delay.
2. The Compose file no longer defines a relay sidecar.
3. The final vehicle image contains the installed relay executable.
4. Existing relay, ControlBridge, and supervisor tests remain green.

Runtime verification is performed with `enable_actuation=false` and no physical
movement test:

1. Only `tod_vehicle_edge` runs for TOD vehicle software; no relay sidecar runs.
2. `/vehicle/can/raw` has exactly one publisher and the control target has one
   subscription.
3. All filtered CAN frame families continue updating with ages below 300 ms.
4. Terminating the relay child process produces a new relay PID after about one
   second while the vehicle container remains running.
5. During relay interruption, no real-control publishers appear and no safety
   latch is bypassed.

## Non-Goals

- Changing CAN frame parsing or the CAN ID allowlist.
- Adding CAN transmission to the relay.
- Relaxing any ControlBridge timeout or safety requirement.
- Combining relay implementation code into the Python ControlBridge process.
- Changing camera, lidar, chassis-driver, or operator deployment behavior.

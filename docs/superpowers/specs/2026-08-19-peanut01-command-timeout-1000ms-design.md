# Peanut01 1000 ms Command Timeout Design

## Goal

Increase the Peanut01 TOD command freshness timeout from 300 ms to 1000 ms so
short Wi-Fi stalls below one second do not immediately latch a control fault.

## Scope

Keep the command timeout consistent in all persistent definitions:

- the shared and ControlBridge-specific YAML parameters;
- the ControlBridge parameter fallback;
- the control supervisor parameter fallback;
- the contract tests that enforce these defaults.

The deployed vehicle image and mounted configuration must both expose
`command_timeout_ms=1000` after the vehicle container is recreated.

## Unchanged Safety Behavior

- `feedback_timeout_ms` remains 300 ms.
- `can_feedback_timeout_ms` remains 300 ms.
- `execution_confirmation_timeout_ms` remains 1000 ms.
- `arming_duration_ms` remains 1000 ms.
- The 0.02 m/s stopped-vehicle threshold remains unchanged.
- Fault latching, explicit disable-before-rearm, and fail-closed publisher
  teardown remain unchanged.
- Actuation defaults to disabled and remains disabled during deployment and
  verification.

## Behavior

ControlBridge treats primary/secondary TOD commands and required status as
fresh for up to 1000 ms. A required input older than 1000 ms still produces
`stale TOD command or status`, tears down real-control publishers, and latches
the existing fault state.

MCU, EPS, CAN, and execution feedback continue to use their existing,
independent timeouts. Increasing the command timeout therefore does not permit
stale physical feedback.

## Verification

1. Add a contract assertion for the 1000 ms command timeout and observe it fail
   against the current 300 ms definitions.
2. Update the minimal YAML and Python defaults, then run the focused control
   tests and the full test suite.
3. Rebuild the vehicle image and recreate only `tod_vehicle` while actuation is
   disabled.
4. Confirm the running ROS parameter is 1000 ms, other safety timeouts retain
   their previous values, no real-control publishers exist, and protected
   driver/chassis/algorithm containers were not restarted.
5. Do not perform a physical movement test as part of deployment.

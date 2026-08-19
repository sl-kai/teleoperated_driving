# Visual Real-Actuation Control Design

## Goal

Add a safety-oriented control in the lower-left corner of the operator Visual
interface so the operator can enable, stop, and reset real vehicle actuation
without running manual ROS parameter commands. The interface must display the
vehicle supervisor's actual state: `DISABLED`, `ARMING`, `ACTIVE`, or `FAULT`.

The UI never decides that actuation is active based on a button press. It only
displays states reported by the vehicle-side supervisor.

## Scope

This change includes:

- A lower-left power button and adjacent status text in Visual.
- A dedicated operator-to-vehicle actuation control service.
- A dedicated vehicle-to-operator actuation state message.
- Transport integration using the existing TCP service forwarding and TOD data
  transport patterns.
- Vehicle bridge integration with the existing supervisor and
  `enable_actuation` behavior.
- Tests for UI interaction, transport contracts, and supervisor safety.

This change does not weaken or replace any existing vehicle safety gate. It
does not add an operation that forces the supervisor directly into `ACTIVE`.

## User Interface

The control is fixed in the lower-left corner of the Visual drive interface.
It consists of a stable-size power icon button with status text immediately to
its right. The icon follows the supplied reference image's familiar power
symbol but uses a new monochrome bitmap that Visual can tint per state.

The status presentation is:

| State | Appearance | Interaction |
| --- | --- | --- |
| `DISABLED` | Gray | Hold for two seconds to request enable |
| `ARMING` | Pulsing amber | Click to cancel and disable immediately |
| `ACTIVE` | Steady blue | Click to disable immediately |
| `FAULT` | Pulsing red | Hold for two seconds to reset to `DISABLED` |
| No fresh feedback | Gray, disabled, `NO DATA` | No enable action allowed |

A circular progress indicator around the button shows hold progress. Releasing
before two seconds sends no request. While a request is in flight, the button
is locked against duplicate requests and continues displaying the last
vehicle-confirmed state.

For `FAULT`, the supervisor reason is displayed below the state. Long-pressing
in `FAULT` sends a disable request, which clears the existing fault latch. It
does not automatically send a subsequent enable request. The operator must
hold again from `DISABLED` to re-arm.

State feedback older than one second is considered stale. Visual then displays
`NO DATA`, disables enable/reset interaction, and does not retain a misleading
`ACTIVE` indication.

## Messages And Services

Add a dedicated service with this logical contract:

```text
SetActuationEnabled.srv
bool enable
---
bool accepted
uint8 current_state
string reason
```

Add a dedicated state message with this logical contract:

```text
ActuationControlState.msg
std_msgs/Header header
uint8 DISABLED=0
uint8 ARMING=1
uint8 ACTIVE=2
uint8 FAULT=3
uint8 state
bool enable_requested
string reason
```

The concrete message package will follow the existing ownership convention for
vehicle control interfaces. Both service and state use generated state
constants instead of UI-specific strings.

## Data Flow

The enable/disable request follows this path:

```text
Visual power button
  -> operator actuation service client
  -> dedicated TCP service forwarder
  -> vehicle service listener
  -> ControlBridge actuation service
  -> existing Supervisor.request_enable(enable)
```

The state feedback follows this path:

```text
ControlBridge supervisor decision
  -> vehicle ActuationControlState publisher
  -> existing TOD vehicle-to-operator data transport
  -> operator ActuationControlState receiver
  -> Visual subscriber component
  -> lower-left button and status text
```

The service request is not sent through cross-host ROS discovery. The current
deployment does not expose the vehicle `ControlBridge` parameter services to
the operator. The dedicated TCP forwarder gives the request an explicit,
testable transport path consistent with existing monitoring and video config
services.

## Vehicle Behavior

The vehicle service delegates to the existing supervisor semantics:

- `enable=true` requests `ARMING` only.
- `enable=false` requests stop/manual mode and returns the supervisor to
  `DISABLED`, including clearing a latched `FAULT`.
- A rejected or unsafe enable request never bypasses readiness checks.
- The existing one-second arming duration, command timeout, execution feedback
  confirmation, neutral/zero-motion requirements, emergency status, MCU
  feedback, approval checks, and local override checks remain unchanged.

The bridge publishes state on transitions and periodically while running so
the operator can detect stale or lost feedback. The response reports whether
the request was accepted for processing, the state at response time, and a
reason. A successful response does not imply `ACTIVE`; only later state
feedback can establish that.

## Failure Handling

- Enable service timeouts are reported in the UI and are not retried
  automatically.
- Duplicate input is ignored while a request is pending.
- Disable remains available with a single click in `ARMING` and `ACTIVE`.
- If state feedback is stale, enable and reset are disabled.
- A network disconnect disables the UI control. Vehicle-side command timeout
  and fault latching continue to enforce physical safety independently.
- Unsupported state values are displayed as `NO DATA`, never as `ACTIVE`.
- Service rejection leaves the displayed state tied to vehicle feedback and
  shows the returned reason.

## Testing

Tests must cover:

- Message and service definitions and generated constants.
- Operator forwarder and vehicle listener configuration and launch behavior.
- ControlBridge delegation to the existing supervisor without bypassing gates.
- Periodic publication and transition publication of all four states.
- Two-second hold completion and early release behavior.
- Single-click disable in `ARMING` and `ACTIVE`.
- `FAULT` long hold issuing disable only, followed by explicit re-arming.
- State-specific text and button appearance contracts.
- One-second stale feedback behavior and unknown state handling.
- Service rejection, timeout, and duplicate-request suppression.
- Regression coverage for all existing supervisor safety tests.

## Deployment And Rollback

Both operator and vehicle images must be rebuilt because the transport contract
and message types are shared across them. Deployment must verify the service
path, status topic publisher/subscriber endpoints, UI state transitions, and
that the vehicle still starts with actuation disabled.

Rollback uses the currently deployed operator and vehicle image tags. No
configuration default changes to `enable_actuation=true`; a restart must remain
safe and begin in `DISABLED`.

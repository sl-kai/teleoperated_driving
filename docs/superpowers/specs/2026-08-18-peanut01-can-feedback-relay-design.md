# Peanut01 CAN Feedback Relay Design

## Goal

Provide the existing Peanut01 TOD control bridge with verified, read-only
SocketCAN feedback from `can0` by publishing the bridge's required JSON
envelope on `/vehicle/can/raw` in ROS domain 0. The relay must never command
the vehicle or send a CAN frame.

## Scope

The work adds one C++ ROS 2 sidecar named `tod_can_feedback_relay`. It is
separate from `tod_vehicle_edge`, `minguo`, and `peanut01-drivers`. It does
not change the real-control safety gate, CAN command writer, actuation
parameter, or vehicle launch.

## Data Flow

```text
can0 SocketCAN receive queue
  -> tod_can_feedback_relay (read-only filter)
  -> /vehicle/can/raw (std_msgs/msg/String, ROS domain 0)
  -> /peanut01_control_target
  -> existing Peanut01 ControlBridge safety gate
```

The relay accepts only standard, non-RTR, non-error, eight-byte frames with
CAN IDs `0x1A1`, `0x1A2`, `0x1A3`, `0x1A4`, or `0x401`. It discards every other
frame before publishing.

## Message Contract

Each accepted frame is published as compact JSON with exactly these fields:

```json
{
  "stamp_ns": 0,
  "interface": "can0",
  "id": 418,
  "id_hex": "0x1A2",
  "is_extended": false,
  "is_rtr": false,
  "is_error": false,
  "dlc": 8,
  "data_hex": "0011223344556677"
}
```

`stamp_ns` is the relay's monotonic receive timestamp. `data_hex` contains
exactly 16 uppercase hexadecimal characters. This contract matches the strict
validation in `work/peanut01_can_feedback.py`.

## Runtime Behavior

The relay opens `PF_CAN` / `SOCK_RAW` with `CAN_RAW_FILTER` set to the five
accepted IDs and `CAN_RAW_RECV_OWN_MSGS` disabled. It binds to the configured
interface, defaulting to `can0`, and publishes with reliable QoS and depth
500. When interface opening, polling, or receiving fails, it logs the error,
closes the socket, and retries with bounded exponential backoff. It never
publishes a synthetic frame.

The implementation contains no CAN write operation or command-mapping code.
Linux does not expose a separate read-only capability for raw CAN sockets, so
the safety boundary is enforced by a minimal audited implementation, filtering,
read-only filesystem, and a dedicated container. `NET_RAW` remains necessary
to open the raw receive socket.

## Deployment

The vehicle image builds and includes the C++ executable. A new Compose
service, `tod_can_feedback_relay`, uses that image with host networking,
`NET_RAW`, `read_only: true`, and `restart: unless-stopped`. It receives
`ROS_DOMAIN_ID=0` and `CAN_INTERFACE=can0`. It mounts no CAN device path and
does not need access to `/userdata` configuration.

The sidecar can start while `tod_vehicle_edge` is already running; DDS
discovery will connect it dynamically. Deployment must not restart `minguo`,
`peanut01-drivers`, or `tod_vehicle_edge`. It must leave
`enable_actuation=false`.

## Validation

Automated tests cover frame filtering, exact JSON schema, ID and payload
formatting, and the absence of a CAN transmit call in the relay source. A
container-level smoke check verifies that the sidecar starts with `can0`.

On the vehicle, verification is read-only:

1. `/vehicle/can/raw` has exactly one publisher in ROS domain 0.
2. A sampled message matches the defined JSON schema and one recognized ID.
3. The control bridge diagnostic reports finite MCU feedback ages rather than
   `unknown`.
4. `enable_actuation` remains `false`; no vehicle movement test is performed.


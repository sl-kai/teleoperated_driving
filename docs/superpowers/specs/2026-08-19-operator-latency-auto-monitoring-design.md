# Operator Latency Auto-Monitoring Design

## Goal

Automatically populate the Visual application's lower-right latency display whenever the operator establishes a TOD connection. The target must follow the vehicle IP entered in the Manager instead of using a fixed deployment address.

## Current Behavior

The operator network monitor and the Visual subscription already exist. Visual subscribes to `/operator/monitoring/output/network_metrics`, but the local monitor does not publish until its service is enabled.

When Manager reaches `CONNECTED`, `NetworkMonitorComponent` currently enables only the forwarded vehicle-side monitor. It sends the selected operator IP so the vehicle can measure its path back to the operator. No request enables the operator-side monitor, so the Visual latency remains at its initial value.

## Design

`NetworkMonitorComponent` will own two service clients:

- A local client for `/operator/monitoring/network_monitor/set_monitoring_status`.
- The existing forwarded client for the operator-to-vehicle monitoring service.

Its public operation will accept the operator IP, vehicle IP, and desired active state. On a successful transition to `CONNECTED`:

1. The local client enables operator-side monitoring with the vehicle IP entered in Manager.
2. The forwarded client enables vehicle-side monitoring with the selected operator IP.
3. The local monitor publishes metrics on `/operator/monitoring/output/network_metrics`.
4. Visual receives those metrics through its existing remapping and updates the lower-right latency value.

On a successful transition to `DISCONNECTED`, both monitoring services receive disable requests. This preserves the current connection-scoped monitoring lifecycle and avoids generating monitoring traffic while disconnected.

## Error Handling

The two service requests are independent. An unavailable service or a negative response will be logged with enough detail to identify whether the local or vehicle-side monitor failed. Monitoring failure must not block or roll back the TOD connection state because latency display is observational and must not affect control availability.

The implementation will not add retries, a background helper process, a fixed vehicle IP, or a new ROS topic. A later connection attempt naturally provides another opportunity to enable monitoring.

## Compatibility

The existing vehicle-side monitoring request remains active and keeps its current IP direction. Existing network metrics forwarding and Manager consumers are unchanged. Only the `NetworkMonitorComponent` call contract and the two Manager transition call sites change.

## Testing

Contract tests will verify that:

- `NetworkMonitorComponent` creates both local and forwarded service clients.
- The connected transition passes the selected operator IP and entered vehicle IP in the correct order and enables monitoring.
- The disconnected transition passes the same IP pair and disables monitoring.
- Visual remains subscribed to the local operator network metrics topic.

Runtime verification on `wufan@192.168.68.69` will confirm:

- The operator monitor uses `wlp5s0`.
- Connecting to `192.168.68.32` causes `/operator/monitoring/output/network_metrics` to publish continuously.
- Published latency is nonzero when ICMP replies are available.
- Visual is a subscriber to that topic.
- Disconnecting stops publication, and reconnecting starts it again without a manual service call.

## Deployment

Build and deploy an updated operator image to `wufan@192.168.68.69`, recreate only `tod_operator_edge`, and leave the vehicle and its control state untouched. Preserve a rollback tag for the currently running operator image before replacement.

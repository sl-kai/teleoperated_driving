# Fault Snapshot Design

## Goal

Preserve the first safety decision that transitions the Peanut01 control bridge
to `FAULT`, so the root cause remains available after the bridge starts
publishing its normal latched-fault status.

## Scope

The bridge will freeze a snapshot on the first `FAULT` transition during one
actuation request. The snapshot contains the original `decision.reason`, the
transition timestamp, and the input/feedback values used for that decision.
It will be included in the existing bridge diagnostics and emitted once through
the container log.

The snapshot is cleared only when the operator explicitly disables actuation.
It is held in memory and is not retained across a container restart.

## Data

The frozen fields include command and status ages, vehicle feedback age,
teleoperation and autonomous-mode flags, emergency and local-override state,
actuation approvals, requested command values, and MCU/CAN feedback values.

## Behavior

The normal diagnostics `message` remains the current state-machine reason.
Additional diagnostic fields expose the frozen first-fault reason and snapshot.
When a new fault is first detected, one error log records the complete frozen
snapshot. Subsequent timer cycles do not overwrite it.

## Verification

Unit tests will verify that a fault captures its original reason, later
latched-fault cycles retain that reason, and an explicit disable clears it.

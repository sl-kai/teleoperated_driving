# G923 Custom Input Mapping

## Goal

Publish four additional, independent logical buttons and the clutch axis from
the Logitech G923 without changing existing control behavior or sending any
vehicle command.

## Logical Button Layout

The existing logical joystick buttons `0` through `8` retain their current
semantics. New button inputs use the unused positions `9` through `12`:

| Logical button | Physical G923 input |
| --- | --- |
| `CUSTOM_O` (9) | button 2 |
| `CUSTOM_X` (10) | button 0 |
| `CUSTOM_SQUARE` (11) | button 1 |
| `CUSTOM_TRIANGLE` (12) | button 3 |

The clutch uses the unused logical joystick axis `3` and preserves its
continuous raw range from approximately `-1.0` when released to `1.0` when
fully pressed. D-pad axes 4 and 5 are intentionally not mapped.

## Components

- Extend `joystick::ButtonPos` with the four logical button positions and
  `joystick::AxesPos` with `CLUTCH` at position 3.
- Extend the input-device configuration parameters and `logitechg923.yaml`
  with the four physical buttons and clutch axis 1.
- Extend `InputDeviceController` to map the physical button events and clutch
  axis directly.

## Safety and Compatibility

No direct-control, actuation, vehicle-side, or network-command component is
changed. The new values are published only in
`/operator/input_devices/output/joystick`. Existing logical button and axis
indices remain unchanged.

## Verification

- Unit tests cover physical button mapping and clutch-axis mapping.
- The existing input-device test suite passes.
- On the operator, each physical input changes only its designated logical
  `Joy.buttons` entry, with no vehicle-control message created.

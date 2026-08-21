# G923 Custom Input Mapping

## Goal

Publish eight additional, independent logical inputs from the Logitech G923
without changing existing control behavior or sending any vehicle command.

## Logical Button Layout

The existing logical joystick buttons `0` through `8` retain their current
semantics. New inputs use the unused positions `9` through `16`:

| Logical button | Physical G923 input |
| --- | --- |
| `CUSTOM_O` (9) | button 2 |
| `CUSTOM_X` (10) | button 0 |
| `CUSTOM_SQUARE` (11) | button 1 |
| `CUSTOM_TRIANGLE` (12) | button 3 |
| `DPAD_LEFT` (13) | axis 4 less than -0.5 |
| `DPAD_RIGHT` (14) | axis 4 greater than 0.5 |
| `DPAD_UP` (15) | axis 5 less than -0.5 |
| `DPAD_DOWN` (16) | axis 5 greater than 0.5 |

Axis-derived logical buttons return to `0` when the D-pad axis is inside the
threshold range. Opposing directions on the same axis are mutually exclusive.

## Components

- Extend `joystick::ButtonPos` with the eight logical positions.
- Extend the input-device configuration parameters and `logitechg923.yaml`
  with the four physical buttons and two D-pad axes.
- Extend `InputDeviceController` to map physical button events directly and
  translate the two configured D-pad axes into thresholded button states.

## Safety and Compatibility

No direct-control, actuation, vehicle-side, or network-command component is
changed. The new values are published only in
`/operator/input_devices/output/joystick`. Existing logical button and axis
indices remain unchanged.

## Verification

- Unit tests cover physical button mapping and D-pad threshold transitions.
- The existing input-device test suite passes.
- On the operator, each physical input changes only its designated logical
  `Joy.buttons` entry, with no vehicle-control message created.

# G923 Custom Inputs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish four new G923 face-button states in `Joy.buttons[9..12]` and the clutch pedal in `Joy.axes[3]`, without producing any control command.

**Architecture:** The input-device package continues to publish the single `/operator/input_devices/output/joystick` message. New configuration parameters identify the G923's raw inputs; `InputDeviceController` translates those raw events into new logical enum positions. No consumer is changed, so the values remain inert until a later feature explicitly subscribes to them.

**Tech Stack:** ROS 2 Humble, C++14, `sensor_msgs/msg/Joy`, YAML, Python `unittest`, colcon.

---

## File Structure

- `src/tod_msgs/tod_operator_msgs/include/tod_operator_msgs/joystickConfig.h`: Defines stable logical button and axis positions shared by operator components.
- `src/tod_operator_interface/tod_input_devices/src/ros_interface.cpp`: Declares the new mapping parameters.
- `src/tod_operator_interface/tod_input_devices/src/input_device_controller.cpp`: Loads the parameters and forwards raw events to logical positions.
- `src/tod_operator_interface/tod_input_devices/config/logitechg923.yaml`: Assigns the confirmed G923 raw button and clutch-axis identifiers.
- `tests/test_g923_custom_input_mapping.py`: Statically verifies the complete configured mapping and the inert logical positions.

### Task 1: Add a failing mapping-contract test

**Files:**
- Create: `tests/test_g923_custom_input_mapping.py`

- [ ] **Step 1: Write the failing test**

```python
import pathlib
import unittest


REPO = pathlib.Path(__file__).resolve().parents[1]
CONFIG = REPO / "src/tod_operator_interface/tod_input_devices/config/logitechg923.yaml"
ENUMS = REPO / "src/tod_msgs/tod_operator_msgs/include/tod_operator_msgs/joystickConfig.h"
CONTROLLER = REPO / "src/tod_operator_interface/tod_input_devices/src/input_device_controller.cpp"


class G923CustomInputMappingTests(unittest.TestCase):
    def test_g923_face_buttons_have_dedicated_logical_positions(self):
        config = CONFIG.read_text(encoding="utf-8")
        enums = ENUMS.read_text(encoding="utf-8")

        self.assertIn("CustomO: 2", config)
        self.assertIn("CustomX: 0", config)
        self.assertIn("CustomSquare: 1", config)
        self.assertIn("CustomTriangle: 3", config)
        self.assertIn("CUSTOM_O        = 9", enums)
        self.assertIn("CUSTOM_X        = 10", enums)
        self.assertIn("CUSTOM_SQUARE   = 11", enums)
        self.assertIn("CUSTOM_TRIANGLE = 12", enums)

    def test_g923_clutch_uses_the_reserved_fourth_axis(self):
        config = CONFIG.read_text(encoding="utf-8")
        enums = ENUMS.read_text(encoding="utf-8")
        controller = CONTROLLER.read_text(encoding="utf-8")

        self.assertIn("Clutch: 1", config)
        self.assertIn("CLUTCH          = 3", enums)
        self.assertIn("joystick::AxesPos::CLUTCH", controller)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the new test and verify it fails**

Run:

```bash
python -m unittest tests.test_g923_custom_input_mapping -v
```

Expected: FAIL because the custom parameter names and enum positions do not yet exist.

- [ ] **Step 3: Commit the failing test**

```bash
git add tests/test_g923_custom_input_mapping.py
git commit -m "test: specify G923 custom input mapping"
```

### Task 2: Define and publish the new logical inputs

**Files:**
- Modify: `src/tod_msgs/tod_operator_msgs/include/tod_operator_msgs/joystickConfig.h`
- Modify: `src/tod_operator_interface/tod_input_devices/src/ros_interface.cpp`
- Modify: `src/tod_operator_interface/tod_input_devices/src/input_device_controller.cpp`
- Modify: `src/tod_operator_interface/tod_input_devices/config/logitechg923.yaml`

- [ ] **Step 1: Extend the public logical enum values without changing existing positions**

```cpp
enum ButtonPos {
    INDICATOR_LEFT  = 0,
    INDICATOR_RIGHT = 1,
    FLASHLIGHT      = 2,
    FRONTLIGHT      = 3,
    HONK            = 4,
    INCREASE_SPEED  = 5,
    DECREASE_SPEED  = 6,
    INCREASE_GEAR   = 7,
    DECREASE_GEAR   = 8,
    CUSTOM_O        = 9,
    CUSTOM_X        = 10,
    CUSTOM_SQUARE   = 11,
    CUSTOM_TRIANGLE = 12
};

enum AxesPos {
    STEERING        = 0,
    THROTTLE        = 1,
    BRAKE           = 2,
    CLUTCH          = 3
};
```

- [ ] **Step 2: Declare the five new input-device configuration parameters**

```cpp
this->declare_parameter<int>("button_config.CustomO", -1);
this->declare_parameter<int>("button_config.CustomX", -1);
this->declare_parameter<int>("button_config.CustomSquare", -1);
this->declare_parameter<int>("button_config.CustomTriangle", -1);
this->declare_parameter<int>("axis_config.Clutch", -1);
```

- [ ] **Step 3: Add controller mappings for the parameters**

```cpp
int par_custom_o = _ros->get_parameter("button_config.CustomO").get_parameter_value().get<int>();
int par_custom_x = _ros->get_parameter("button_config.CustomX").get_parameter_value().get<int>();
int par_custom_square = _ros->get_parameter("button_config.CustomSquare").get_parameter_value().get<int>();
int par_custom_triangle = _ros->get_parameter("button_config.CustomTriangle").get_parameter_value().get<int>();
int par_axis_clutch = _ros->get_parameter("axis_config.Clutch").get_parameter_value().get<int>();

if (par_custom_o >= 0)
    _button_mapping[par_custom_o] = joystick::ButtonPos::CUSTOM_O;
if (par_custom_x >= 0)
    _button_mapping[par_custom_x] = joystick::ButtonPos::CUSTOM_X;
if (par_custom_square >= 0)
    _button_mapping[par_custom_square] = joystick::ButtonPos::CUSTOM_SQUARE;
if (par_custom_triangle >= 0)
    _button_mapping[par_custom_triangle] = joystick::ButtonPos::CUSTOM_TRIANGLE;
if (par_axis_clutch >= 0)
    _axis_mapping.insert(std::make_pair(par_axis_clutch, AxisItem(joystick::AxesPos::CLUTCH, false)));
```

The `-1` defaults preserve compatibility with all non-G923 YAML files. Do not
add a consumer or publisher beyond the existing `Joy` publisher.

- [ ] **Step 4: Configure the confirmed G923 raw identifiers**

```yaml
button_config:
  CustomO: 2
  CustomX: 0
  CustomSquare: 1
  CustomTriangle: 3

axis_config:
  Clutch: 1
```

- [ ] **Step 5: Run the mapping-contract test and verify it passes**

Run:

```bash
python -m unittest tests.test_g923_custom_input_mapping -v
```

Expected: PASS with two tests.

- [ ] **Step 6: Commit the implementation**

```bash
git add src/tod_msgs/tod_operator_msgs/include/tod_operator_msgs/joystickConfig.h src/tod_operator_interface/tod_input_devices/src/ros_interface.cpp src/tod_operator_interface/tod_input_devices/src/input_device_controller.cpp src/tod_operator_interface/tod_input_devices/config/logitechg923.yaml tests/test_g923_custom_input_mapping.py
git commit -m "feat: publish G923 custom inputs"
```

### Task 3: Build and verify that the inputs remain inert

**Files:**
- Test: `tests/test_usb_input_device.py`
- Test: `tests/test_g923_custom_input_mapping.py`

- [ ] **Step 1: Run the focused Python tests**

Run:

```bash
python -m unittest tests.test_usb_input_device tests.test_g923_custom_input_mapping -v
```

Expected: PASS with all tests.

- [ ] **Step 2: Build the changed ROS packages**

Run:

```bash
colcon build --packages-select tod_operator_msgs tod_input_devices --symlink-install
```

Expected: both packages finish successfully.

- [ ] **Step 3: Verify the live operator after deployment**

Run on the operator host:

```bash
docker compose exec -T tod_operator bash -lc '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic echo /operator/input_devices/output/joystick
'
```

Expected: O/X/方形/三角 alter only `buttons[9]` through `buttons[12]`; clutch alters only `axes[3]`; no actuation parameter or vehicle command topic is changed.

- [ ] **Step 4: Commit verification-only changes if any exist**

```bash
git status --short
```

Expected: no tracked verification artifact requires a commit.

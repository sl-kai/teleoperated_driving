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

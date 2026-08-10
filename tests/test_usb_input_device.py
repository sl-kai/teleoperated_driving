import pathlib
import unittest


REPO = pathlib.Path(__file__).resolve().parents[1]
USB_INPUT_DEVICE = (
    REPO
    / "src/tod_operator_interface/tod_input_devices/src/usb_input_device/usb_input_device.cpp"
)


class UsbInputDeviceTests(unittest.TestCase):
    def test_select_timeout_is_fully_initialized(self):
        source = USB_INPUT_DEVICE.read_text(encoding="utf-8")

        self.assertIn("struct timeval timeout{};", source)
        self.assertIn("timeout.tv_sec = 0;", source)
        self.assertIn("timeout.tv_usec = 200000;", source)
        self.assertNotIn("timeout.tv_sec = 0.2", source)

    def test_select_errors_do_not_spin(self):
        source = USB_INPUT_DEVICE.read_text(encoding="utf-8")

        self.assertIn("select_result", source)
        self.assertIn("select_result < 0", source)
        self.assertIn("errno != EINTR", source)


if __name__ == "__main__":
    unittest.main()

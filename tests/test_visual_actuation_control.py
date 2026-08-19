import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
STATE_MSG = (
    ROOT / "src/tod_msgs/tod_vehicle_msgs/msg/ActuationControlState.msg"
)
CONTROL_SRV = (
    ROOT / "src/tod_msgs/tod_vehicle_msgs/srv/SetActuationEnabled.srv"
)
VEHICLE_MSGS_CMAKE = ROOT / "src/tod_msgs/tod_vehicle_msgs/CMakeLists.txt"
BRIDGE = ROOT / "work/peanut01_control_bridge.py"


def test_generated_actuation_interfaces_are_registered():
    state = STATE_MSG.read_text(encoding="utf-8")
    service = CONTROL_SRV.read_text(encoding="utf-8")
    cmake = VEHICLE_MSGS_CMAKE.read_text(encoding="utf-8")

    for token in (
        "DISABLED=0",
        "ARMING=1",
        "ACTIVE=2",
        "FAULT=3",
        "std_msgs/Header header",
        "uint8 state",
        "bool enable_requested",
        "string reason",
    ):
        assert token in state
    for token in (
        "bool enable",
        "bool accepted",
        "uint8 current_state",
        "string reason",
    ):
        assert token in service
    assert '"msg/ActuationControlState.msg"' in cmake
    assert '"srv/SetActuationEnabled.srv"' in cmake


def test_bridge_exposes_service_and_confirmed_state():
    bridge = BRIDGE.read_text(encoding="utf-8")

    for token in (
        "ActuationControlState",
        "SetActuationEnabled",
        '"set_actuation_enabled"',
        '"/vehicle/interface/peanut01/actuation_control_state"',
        "self.shared.requested_enable = request.enable",
        "self.actuation_state_publisher.publish(message)",
        "ActuationControlState.DISABLED",
        "ActuationControlState.ARMING",
        "ActuationControlState.ACTIVE",
        "ActuationControlState.FAULT",
    ):
        assert token in bridge


def test_bridge_service_uses_locked_snapshot_and_does_not_bypass_supervisor():
    bridge = BRIDGE.read_text(encoding="utf-8")

    callback_start = bridge.index("def on_set_actuation_enabled")
    callback_end = bridge.index("\n    def ", callback_start + 5)
    callback = bridge[callback_start:callback_end]
    assert "with self.shared.lock:" in callback
    assert "self.shared.requested_enable = request.enable" in callback
    assert "self.supervisor.request_enable" not in callback
    assert "response.current_state = self.shared.actuation_state" in callback
    assert "response.reason = self.shared.actuation_reason" in callback

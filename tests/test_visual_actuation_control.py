import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
STATE_MSG = (
    ROOT / "src/tod_msgs/tod_vehicle_msgs/msg/ActuationControlState.msg"
)
CONTROL_SRV = (
    ROOT / "src/tod_msgs/tod_vehicle_msgs/srv/SetActuationEnabled.srv"
)
VEHICLE_MSGS_CMAKE = ROOT / "src/tod_msgs/tod_vehicle_msgs/CMakeLists.txt"


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

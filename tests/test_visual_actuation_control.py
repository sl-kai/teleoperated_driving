import pathlib

import yaml


ROOT = pathlib.Path(__file__).resolve().parents[1]
STATE_MSG = (
    ROOT / "src/tod_msgs/tod_vehicle_msgs/msg/ActuationControlState.msg"
)
CONTROL_SRV = (
    ROOT / "src/tod_msgs/tod_vehicle_msgs/srv/SetActuationEnabled.srv"
)
VEHICLE_MSGS_CMAKE = ROOT / "src/tod_msgs/tod_vehicle_msgs/CMakeLists.txt"
BRIDGE = ROOT / "work/peanut01_control_bridge.py"
COMMUNICATION = ROOT / "src/tod_network/tod_communication_interface"
SERVICES_CONFIG = (
    ROOT
    / "config/config/package_config/tod_communication_interface/services.yaml"
)
OPERATOR_DATA_CONFIG = (
    ROOT / "config/config/package_config/tod_data_interface/operator_config.yaml"
)
VEHICLE_DATA_CONFIG = (
    ROOT / "config/config/package_config/tod_data_interface/vehicle_config.yaml"
)
REMAPPINGS = ROOT / "config/config/remappings.yaml"


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


def test_actuation_service_has_dedicated_tcp_forwarder_and_listener():
    services = yaml.safe_load(SERVICES_CONFIG.read_text(encoding="utf-8"))[
        "services"
    ]
    service = services["ActuationControlService"]
    assert service == {
        "protocol": "TCP",
        "service_vehicle": "from_operator/set_actuation_enabled",
        "service_operator": "to_vehicle/set_actuation_enabled",
        "forwarder_port": 60500,
        "listener_port": 60501,
    }

    operator_launch = (
        COMMUNICATION / "launch/tod_communication_interface_operator.launch.py"
    ).read_text(encoding="utf-8")
    vehicle_launch = (
        COMMUNICATION / "launch/tod_communication_interface_vehicle.launch.py"
    ).read_text(encoding="utf-8")
    assert "ActuationControlServiceForwarder" in operator_launch
    assert "ActuationControlServiceListener" in vehicle_launch

    forwarder = (
        COMMUNICATION
        / "src/operator/actuation_control_service_forwarder.cpp"
    ).read_text(encoding="utf-8")
    listener = (
        COMMUNICATION
        / "src/vehicle/actuation_control_service_listener.cpp"
    ).read_text(encoding="utf-8")
    service_type = "tod_vehicle_msgs::srv::SetActuationEnabled"
    assert f"ServiceForwarder<{service_type}>" in forwarder
    assert f"ServiceListener<{service_type}>" in listener


def test_actuation_state_uses_always_on_vehicle_to_operator_transport():
    operator = yaml.safe_load(OPERATOR_DATA_CONFIG.read_text(encoding="utf-8"))
    vehicle = yaml.safe_load(VEHICLE_DATA_CONFIG.read_text(encoding="utf-8"))

    receiver = next(
        item
        for item in operator["receivers"]
        if item["name"] == "ActuationControlStateReceiver"
    )
    sender = next(
        item
        for item in vehicle["senders"]
        if item["name"] == "ActuationControlStateSender"
    )
    assert receiver["port"] == 60012
    assert sender["port"] == 60012
    assert receiver["topic_type"] == "tod_vehicle_msgs/msg/ActuationControlState"
    assert sender["topic_type"] == "tod_vehicle_msgs/msg/ActuationControlState"
    assert sender["send_always"] is True
    assert 99 in sender["sending_control_modes"]


def test_actuation_service_and_state_remappings_are_explicit():
    remappings = yaml.safe_load(REMAPPINGS.read_text(encoding="utf-8"))
    communication_pairs = {
        (item["from"], item["to"])
        for item in remappings["tod_communication_interface"]
    }
    data_pairs = {
        (item["from"], item["to"])
        for item in remappings["tod_data_interface"]
    }
    visual_pairs = {
        (item["from"], item["to"])
        for item in remappings["tod_visual"]
    }
    assert (
        "/vehicle/network/config/from_operator/set_actuation_enabled",
        "/vehicle/interface/peanut01/ControlBridge/set_actuation_enabled",
    ) in communication_pairs
    assert (
        "/vehicle/network/data/to_operator/actuation_control_state",
        "/vehicle/interface/peanut01/actuation_control_state",
    ) in data_pairs
    assert (
        "/operator/interface/visual/input/actuation_control_state",
        "/operator/network/data/from_vehicle/actuation_control_state",
    ) in visual_pairs

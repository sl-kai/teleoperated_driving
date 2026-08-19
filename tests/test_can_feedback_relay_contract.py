import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/tod_can_feedback_relay/src/can_feedback_relay_node.cpp"
LAUNCH = ROOT / "src/tod_launch/launch/tod_vehicle_peanut01_video.launch.py"
DOCKERFILE = ROOT / "docker/dockerfile"
COMPOSE = ROOT / "docker-compose.yaml"


def test_relay_is_receive_only_and_publishes_required_topic():
    text = SOURCE.read_text(encoding="utf-8")

    for token in (
        "PF_CAN",
        "SOCK_RAW",
        "CAN_RAW_FILTER",
        "CAN_RAW_RECV_OWN_MSGS",
        "/vehicle/can/raw",
        "0x1A1",
        "0x1A2",
        "0x1A3",
        "0x1A4",
        "0x401",
    ):
        assert token in text

    for forbidden in ("send(", "sendto(", "sendmsg(", "write("):
        assert forbidden not in text


def test_relay_drains_the_nonblocking_socket_queue():
    text = SOURCE.read_text(encoding="utf-8")

    assert "SOCK_NONBLOCK" in text
    assert "while (true)" in text
    assert "EAGAIN" in text
    assert "EWOULDBLOCK" in text


def test_vehicle_launch_runs_relay_in_domain_zero_with_respawn():
    text = LAUNCH.read_text(encoding="utf-8")

    assert 'package="tod_can_feedback_relay"' in text
    assert 'executable="tod_can_feedback_relay"' in text
    assert '"interface": "can0"' in text
    assert 'additional_env={"ROS_DOMAIN_ID": "0"}' in text
    assert "respawn=True" in text
    assert "respawn_delay=1.0" in text


def test_vehicle_image_requires_installed_relay_executable():
    text = DOCKERFILE.read_text(encoding="utf-8")

    assert (
        "install/tod_can_feedback_relay/lib/tod_can_feedback_relay/"
        "tod_can_feedback_relay"
    ) in text


def test_compose_has_no_relay_sidecar():
    text = COMPOSE.read_text(encoding="utf-8")

    assert "tod_can_feedback_relay:" not in text
    assert "TOD_CAN_FEEDBACK_RELAY_IMAGE" not in text

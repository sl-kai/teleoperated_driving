import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/tod_can_feedback_relay/src/can_feedback_relay_node.cpp"


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

import pathlib
import re

import yaml


ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / (
    "src/tod_operator_interface/tod_visual/src/tod_gl/include/tod_gl/"
    "ros_interface/service_components/network_monitor_component.hpp"
)
COMPONENT = ROOT / (
    "src/tod_operator_interface/tod_visual/src/tod_gl/src/ros_interface/"
    "service_components/network_monitor_component.cpp"
)
STATE_LAYER = ROOT / (
    "src/tod_operator_interface/tod_visual/src/tod_applications/manager/"
    "src/application_layer/src/operator_state_layer.cpp"
)
REMAPPINGS = ROOT / "config/config/remappings.yaml"


def test_network_monitor_component_owns_local_and_vehicle_clients():
    header = HEADER.read_text(encoding="utf-8")
    source = COMPONENT.read_text(encoding="utf-8")

    assert "local_client_" in header
    assert "vehicle_client_" in header
    assert (
        '"/operator/monitoring/network_monitor/set_monitoring_status"'
        in source
    )
    assert "to_vehicle/set_monitoring_status" in source


def test_manager_starts_and_stops_both_monitors_with_directional_ips():
    state_layer = re.sub(r"\s+", " ", STATE_LAYER.read_text(encoding="utf-8"))

    assert re.search(
        r"_networkMonitor\.SetMonitorStatus\(\s*"
        r"ipOperatorOptions\[selectedOperatorIP\],\s*inputBuffer,\s*true\s*\);",
        state_layer,
    )
    assert re.search(
        r"_networkMonitor\.SetMonitorStatus\(\s*"
        r"ipOperatorOptions\[selectedOperatorIP\],\s*inputBuffer,\s*false\s*\);",
        state_layer,
    )


def test_visual_keeps_using_local_operator_metrics():
    remappings = yaml.safe_load(REMAPPINGS.read_text(encoding="utf-8"))
    pairs = {
        (entry["from"], entry["to"])
        for entry in remappings["tod_visual"]
    }

    assert (
        "/operator/interface/visual/input/network_metrics",
        "/operator/monitoring/output/network_metrics",
    ) in pairs

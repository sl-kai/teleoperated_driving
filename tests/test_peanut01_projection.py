import pathlib

import pytest
import yaml


ROOT = pathlib.Path(__file__).resolve().parents[1]
PROFILE = ROOT / "config/config/launch_setup_peanut01_video.yaml"
PARAMS = ROOT / "config/config/package_config/tod_projection/params.yaml"
VEHICLE = ROOT / "config/config/vehicle_config/peanut01/vehicle-params.yaml"
REMAPPINGS = ROOT / "config/config/remappings.yaml"
LAUNCH = (
    ROOT
    / "src/tod_perception/tod_projection/launch/tod_projection.launch.py"
)


def load_yaml(path):
    return yaml.safe_load(path.read_text(encoding="utf-8"))


def test_peanut01_video_profile_enables_projection():
    profile = load_yaml(PROFILE)

    assert profile["packages_to_launch"]["operator"]["tod_projection"] is True
    assert profile["packages_to_launch"]["both"]["tod_trajectory_guidance"] is False


def test_peanut01_uses_rear_axle_and_measured_tire_angle():
    params = load_yaml(PARAMS)["/**"]["ros__parameters"]

    assert params == {
        "steering_angle_source": "steering_tire_angle",
        "kinematic_reference": "rear_axle",
        "prediction_length_m": 6.0,
        "prediction_steps": 40,
    }


def test_peanut01_geometry_derives_confirmed_rear_axle_footprint():
    vehicle = load_yaml(VEHICLE)
    wheelbase = vehicle["distance_front_axle"] + vehicle["distance_rear_axle"]
    front_overhang = (
        vehicle["distance_front_bumper"] - vehicle["distance_front_axle"]
    )
    rear_overhang = (
        vehicle["distance_rear_bumper"] - vehicle["distance_rear_axle"]
    )

    assert wheelbase == pytest.approx(0.80)
    assert front_overhang == pytest.approx(0.295)
    assert rear_overhang == pytest.approx(0.5778)
    assert wheelbase + front_overhang == pytest.approx(1.095)
    assert vehicle["width_edge_to_edge"] / 2.0 == pytest.approx(0.5965)
    assert vehicle["maximum_road_wheel_angle"] == pytest.approx(1.047)


def test_projection_launch_loads_deployment_parameter_file():
    launch = LAUNCH.read_text(encoding="utf-8")

    assert "PathJoinSubstitution" in launch
    assert "'package_config'" in launch
    assert "'tod_projection'" in launch
    assert "'params.yaml'" in launch
    assert "node_param_path" in launch


def test_projection_outputs_are_remapped_to_all_visual_driving_lanes():
    remappings = load_yaml(REMAPPINGS)["tod_projection"]
    pairs = {(item["from"], item["to"]) for item in remappings}

    assert {
        (
            "/operator/projection/output/vehicle_lane_front_left",
            "/operator/interface/visual/input/driving_lane_front_left",
        ),
        (
            "/operator/projection/output/vehicle_lane_front_right",
            "/operator/interface/visual/input/driving_lane_front_right",
        ),
        (
            "/operator/projection/output/vehicle_lane_rear_left",
            "/operator/interface/visual/input/driving_lane_rear_left",
        ),
        (
            "/operator/projection/output/vehicle_lane_rear_right",
            "/operator/interface/visual/input/driving_lane_rear_right",
        ),
    }.issubset(pairs)

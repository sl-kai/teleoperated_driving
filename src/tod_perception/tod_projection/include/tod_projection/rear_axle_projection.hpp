#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace tod_projection {

enum class SteeringAngleSource {
  SteeringWheelAngle,
  SteeringTireAngle,
};

enum class KinematicReference {
  LegacyCenter,
  RearAxle,
};

struct Geometry {
  double wheelbase;
  double front_overhang;
  double rear_overhang;
  double width;
  double maximum_tire_angle;
};

struct Point {
  double x;
  double y;
};

struct Paths {
  std::vector<Point> front_left;
  std::vector<Point> front_right;
  std::vector<Point> rear_left;
  std::vector<Point> rear_right;
};

Paths project(
  const Geometry & geometry, double tire_angle, int direction,
  double prediction_length, std::size_t prediction_steps);

SteeringAngleSource parse_steering_angle_source(const std::string & value);
KinematicReference parse_kinematic_reference(const std::string & value);
double select_steering_angle(
  SteeringAngleSource source, double steering_wheel_angle, double steering_tire_angle,
  double maximum_steering_wheel_angle, double maximum_tire_angle);

}  // namespace tod_projection

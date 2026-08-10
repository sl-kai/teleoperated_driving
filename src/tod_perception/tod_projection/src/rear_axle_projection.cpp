#include "tod_projection/rear_axle_projection.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tod_projection {
namespace {

void validate(
  const Geometry & geometry, double tire_angle, int direction,
  double prediction_length, std::size_t prediction_steps)
{
  if (!std::isfinite(geometry.wheelbase) || geometry.wheelbase <= 0.0) {
    throw std::invalid_argument("wheelbase must be finite and positive");
  }
  if (!std::isfinite(geometry.front_overhang) || geometry.front_overhang < 0.0) {
    throw std::invalid_argument("front overhang must be finite and non-negative");
  }
  if (!std::isfinite(geometry.rear_overhang) || geometry.rear_overhang < 0.0) {
    throw std::invalid_argument("rear overhang must be finite and non-negative");
  }
  if (!std::isfinite(geometry.width) || geometry.width <= 0.0) {
    throw std::invalid_argument("vehicle width must be finite and positive");
  }
  if (!std::isfinite(geometry.maximum_tire_angle) || geometry.maximum_tire_angle <= 0.0) {
    throw std::invalid_argument("maximum tire angle must be finite and positive");
  }
  if (!std::isfinite(tire_angle)) {
    throw std::invalid_argument("tire angle must be finite");
  }
  if (direction != -1 && direction != 1) {
    throw std::invalid_argument("direction must be -1 or 1");
  }
  if (!std::isfinite(prediction_length) || prediction_length <= 0.0) {
    throw std::invalid_argument("prediction length must be finite and positive");
  }
  if (prediction_steps == 0) {
    throw std::invalid_argument("prediction steps must be positive");
  }
}

Point transform(double local_x, double local_y, double x, double y, double yaw)
{
  return {
    x + local_x * std::cos(yaw) - local_y * std::sin(yaw),
    y + local_x * std::sin(yaw) + local_y * std::cos(yaw),
  };
}

void append_body_corners(
  Paths & paths, const Geometry & geometry, double x, double y, double yaw)
{
  const double front = geometry.wheelbase + geometry.front_overhang;
  const double rear = -geometry.rear_overhang;
  const double half_width = geometry.width / 2.0;

  paths.front_left.push_back(transform(front, half_width, x, y, yaw));
  paths.front_right.push_back(transform(front, -half_width, x, y, yaw));
  paths.rear_left.push_back(transform(rear, half_width, x, y, yaw));
  paths.rear_right.push_back(transform(rear, -half_width, x, y, yaw));
}

}  // namespace

Paths project(
  const Geometry & geometry, double tire_angle, int direction,
  double prediction_length, std::size_t prediction_steps)
{
  validate(geometry, tire_angle, direction, prediction_length, prediction_steps);

  const double angle = std::clamp(
    tire_angle, -geometry.maximum_tire_angle, geometry.maximum_tire_angle);
  const double distance_step = prediction_length / static_cast<double>(prediction_steps);
  const double signed_step = static_cast<double>(direction) * distance_step;

  Paths paths;
  const std::size_t sample_count = prediction_steps + 1;
  paths.front_left.reserve(sample_count);
  paths.front_right.reserve(sample_count);
  paths.rear_left.reserve(sample_count);
  paths.rear_right.reserve(sample_count);

  double x = 0.0;
  double y = 0.0;
  double yaw = 0.0;
  append_body_corners(paths, geometry, x, y, yaw);

  for (std::size_t step = 0; step < prediction_steps; ++step) {
    x += signed_step * std::cos(yaw);
    y += signed_step * std::sin(yaw);
    yaw += signed_step * std::tan(angle) / geometry.wheelbase;
    append_body_corners(paths, geometry, x, y, yaw);
  }

  return paths;
}

SteeringAngleSource parse_steering_angle_source(const std::string & value)
{
  if (value == "steering_wheel_angle") {
    return SteeringAngleSource::SteeringWheelAngle;
  }
  if (value == "steering_tire_angle") {
    return SteeringAngleSource::SteeringTireAngle;
  }
  throw std::invalid_argument(
          "steering_angle_source must be steering_wheel_angle or steering_tire_angle");
}

KinematicReference parse_kinematic_reference(const std::string & value)
{
  if (value == "legacy_center") {
    return KinematicReference::LegacyCenter;
  }
  if (value == "rear_axle") {
    return KinematicReference::RearAxle;
  }
  throw std::invalid_argument(
          "kinematic_reference must be legacy_center or rear_axle");
}

double select_steering_angle(
  SteeringAngleSource source, double steering_wheel_angle, double steering_tire_angle,
  double maximum_steering_wheel_angle, double maximum_tire_angle)
{
  if (source == SteeringAngleSource::SteeringTireAngle) {
    if (!std::isfinite(steering_tire_angle)) {
      throw std::invalid_argument("steering tire angle must be finite");
    }
    return steering_tire_angle;
  }

  if (!std::isfinite(steering_wheel_angle)) {
    throw std::invalid_argument("steering wheel angle must be finite");
  }
  if (!std::isfinite(maximum_steering_wheel_angle) || maximum_steering_wheel_angle <= 0.0) {
    throw std::invalid_argument("maximum steering wheel angle must be finite and positive");
  }
  if (!std::isfinite(maximum_tire_angle) || maximum_tire_angle <= 0.0) {
    throw std::invalid_argument("maximum tire angle must be finite and positive");
  }
  return steering_wheel_angle / maximum_steering_wheel_angle * maximum_tire_angle;
}

}  // namespace tod_projection

#pragma once

#include <cstddef>
#include <vector>

namespace tod_projection {

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

}  // namespace tod_projection

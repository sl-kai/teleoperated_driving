#include "tod_projection/rear_axle_projection.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

tod_projection::Geometry peanut01_geometry()
{
  return {
    0.8,
    0.295,
    0.5778,
    1.193,
    1.047,
  };
}

TEST(RearAxleProjection, StraightPathStartsAtCurrentBodyCorners)
{
  const auto paths = tod_projection::project(peanut01_geometry(), 0.0, 1, 6.0, 40);

  ASSERT_EQ(paths.front_left.size(), 41U);
  ASSERT_EQ(paths.front_right.size(), 41U);
  ASSERT_EQ(paths.rear_left.size(), 41U);
  ASSERT_EQ(paths.rear_right.size(), 41U);
  EXPECT_NEAR(paths.front_left.front().x, 1.095, 1e-9);
  EXPECT_NEAR(paths.front_left.front().y, 0.5965, 1e-9);
  EXPECT_NEAR(paths.rear_right.front().x, -0.5778, 1e-9);
  EXPECT_NEAR(paths.rear_right.front().y, -0.5965, 1e-9);
  EXPECT_NEAR(paths.front_left.back().x, 7.095, 1e-9);
  EXPECT_NEAR(paths.front_left.back().y, 0.5965, 1e-9);
}

TEST(RearAxleProjection, SteeringSignsBendInOppositeDirections)
{
  const auto left = tod_projection::project(peanut01_geometry(), 0.2, 1, 6.0, 40);
  const auto right = tod_projection::project(peanut01_geometry(), -0.2, 1, 6.0, 40);

  EXPECT_GT(left.front_left.back().y, left.front_left.front().y);
  EXPECT_LT(right.front_right.back().y, right.front_right.front().y);
}

TEST(RearAxleProjection, ReverseProgressesBehindRearAxle)
{
  const auto paths = tod_projection::project(peanut01_geometry(), 0.0, -1, 6.0, 40);

  EXPECT_LT(paths.rear_left.back().x, paths.rear_left.front().x);
  EXPECT_NEAR(paths.rear_left.back().x, -6.5778, 1e-9);
}

TEST(RearAxleProjection, TireAngleIsClampedToVehicleLimit)
{
  const auto at_limit = tod_projection::project(peanut01_geometry(), 1.047, 1, 6.0, 40);
  const auto beyond_limit = tod_projection::project(peanut01_geometry(), 2.0, 1, 6.0, 40);

  ASSERT_EQ(at_limit.front_left.size(), beyond_limit.front_left.size());
  for (std::size_t index = 0; index < at_limit.front_left.size(); ++index) {
    EXPECT_NEAR(at_limit.front_left[index].x, beyond_limit.front_left[index].x, 1e-9);
    EXPECT_NEAR(at_limit.front_left[index].y, beyond_limit.front_left[index].y, 1e-9);
  }
}

TEST(RearAxleProjection, InvalidInputsAreRejected)
{
  auto geometry = peanut01_geometry();
  geometry.wheelbase = 0.0;
  EXPECT_THROW(tod_projection::project(geometry, 0.0, 1, 6.0, 40), std::invalid_argument);

  geometry = peanut01_geometry();
  geometry.width = 0.0;
  EXPECT_THROW(tod_projection::project(geometry, 0.0, 1, 6.0, 40), std::invalid_argument);

  EXPECT_THROW(
    tod_projection::project(peanut01_geometry(), 0.0, 1, 0.0, 40), std::invalid_argument);
  EXPECT_THROW(
    tod_projection::project(peanut01_geometry(), 0.0, 1, 6.0, 0), std::invalid_argument);
  EXPECT_THROW(
    tod_projection::project(
      peanut01_geometry(), std::numeric_limits<double>::quiet_NaN(), 1, 6.0, 40),
    std::invalid_argument);
}

}  // namespace

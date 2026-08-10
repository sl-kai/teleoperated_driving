add_library(rear_axle_projection rear_axle_projection.cpp)
target_include_directories(rear_axle_projection PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>)

set(TARGETS ${TARGETS} OperatorLaneProjection rear_axle_projection)
add_executable(OperatorLaneProjection operator_lane_projection.cpp)
target_link_libraries(OperatorLaneProjection rear_axle_projection)
ament_target_dependencies(OperatorLaneProjection rclcpp tod_vehicle_msgs nav_msgs tod_helper tod_core)

install(
    TARGETS ${TARGETS} 
    DESTINATION lib/${PROJECT_NAME})

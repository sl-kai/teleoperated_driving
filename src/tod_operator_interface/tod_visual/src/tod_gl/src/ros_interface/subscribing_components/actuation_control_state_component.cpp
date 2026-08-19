#include "tod_gl/ros_interface/subscribing_components/actuation_control_state_component.hpp"

namespace tod_gl {

void ActuationControlStateComponent::cb_message(
    const tod_vehicle_msgs::msg::ActuationControlState::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state = msg->state;
    snapshot_.enable_requested = msg->enable_requested;
    snapshot_.reason = msg->reason;
    snapshot_.received_at = std::chrono::steady_clock::now();
    snapshot_.received = true;
}

}  // namespace tod_gl

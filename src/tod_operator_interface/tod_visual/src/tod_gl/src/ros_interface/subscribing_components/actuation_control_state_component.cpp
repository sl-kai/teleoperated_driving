#include "tod_gl/ros_interface/subscribing_components/actuation_control_state_component.hpp"

namespace tod_gl {

ActuationControlStateComponent::ActuationControlStateComponent(
    std::shared_ptr<rclcpp::Node> sub_node)
    : state_(std::make_shared<SharedState>()),
      subscription_(sub_node->create_subscription<Message>(
          "input/actuation_control_state", 1,
          [state = state_](const Message::SharedPtr msg) {
              std::lock_guard<std::mutex> lock(state->mutex);
              state->snapshot.state = msg->state;
              state->snapshot.enable_requested = msg->enable_requested;
              state->snapshot.reason = msg->reason;
              state->snapshot.received_at = std::chrono::steady_clock::now();
              state->snapshot.received = true;
          })) {}

}  // namespace tod_gl

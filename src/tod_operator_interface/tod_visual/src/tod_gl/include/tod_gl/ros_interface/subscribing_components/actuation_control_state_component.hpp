#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#include "tod_gl/ros_interface/subscribing_component_base.hpp"
#include "tod_vehicle_msgs/msg/actuation_control_state.hpp"

namespace tod_gl {

struct ActuationControlSnapshot {
    uint8_t state{tod_vehicle_msgs::msg::ActuationControlState::DISABLED};
    bool enable_requested{false};
    std::string reason;
    std::chrono::steady_clock::time_point received_at{};
    bool received{false};
};

class ActuationControlStateComponent
    : public SubscribingComponent<tod_vehicle_msgs::msg::ActuationControlState> {
  public:
    explicit ActuationControlStateComponent(std::shared_ptr<rclcpp::Node> sub_node)
        : SubscribingComponent(sub_node, "input/actuation_control_state") {}

    ActuationControlSnapshot snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_;
    }

    bool is_fresh(
        std::chrono::steady_clock::duration max_age = std::chrono::seconds(1)) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_.received &&
               std::chrono::steady_clock::now() - snapshot_.received_at <= max_age;
    }

  private:
    void cb_message(
        const tod_vehicle_msgs::msg::ActuationControlState::SharedPtr msg) override;

    mutable std::mutex mutex_;
    ActuationControlSnapshot snapshot_;
};

}  // namespace tod_gl

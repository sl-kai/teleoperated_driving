#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "tod_vehicle_msgs/msg/actuation_control_state.hpp"

namespace tod_gl {

struct ActuationControlSnapshot {
    uint8_t state{tod_vehicle_msgs::msg::ActuationControlState::DISABLED};
    bool enable_requested{false};
    std::string reason;
    std::chrono::steady_clock::time_point received_at{};
    bool received{false};
};

class ActuationControlStateComponent {
  public:
    explicit ActuationControlStateComponent(std::shared_ptr<rclcpp::Node> sub_node);

    ActuationControlSnapshot snapshot() const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->snapshot;
    }

    bool is_fresh(
        std::chrono::steady_clock::duration max_age = std::chrono::seconds(1)) const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->snapshot.received &&
               std::chrono::steady_clock::now() - state_->snapshot.received_at <= max_age;
    }

  private:
    using Message = tod_vehicle_msgs::msg::ActuationControlState;

    struct SharedState {
        mutable std::mutex mutex;
        ActuationControlSnapshot snapshot;
    };

    std::shared_ptr<SharedState> state_;
    rclcpp::Subscription<Message>::SharedPtr subscription_;
};

}  // namespace tod_gl

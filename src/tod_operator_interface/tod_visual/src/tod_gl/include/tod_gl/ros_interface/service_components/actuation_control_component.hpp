#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "tod_vehicle_msgs/srv/set_actuation_enabled.hpp"

namespace tod_gl {

class ActuationControlComponent {
  public:
    explicit ActuationControlComponent(std::shared_ptr<rclcpp::Node> node);

    bool request(bool enable);
    void update();
    bool pending() const;
    std::string last_error() const;

  private:
    using Service = tod_vehicle_msgs::srv::SetActuationEnabled;
    using Client = rclcpp::Client<Service>;

    std::shared_ptr<rclcpp::Node> node_;
    Client::SharedPtr client_;
    mutable std::mutex mutex_;
    bool pending_{false};
    uint64_t request_id_{0};
    std::chrono::steady_clock::time_point request_started_{};
    std::string last_error_;
};

}  // namespace tod_gl

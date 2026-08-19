#include "tod_gl/ros_interface/service_components/actuation_control_component.hpp"

#include <exception>
#include <utility>

namespace tod_gl {

ActuationControlComponent::ActuationControlComponent(
    std::shared_ptr<rclcpp::Node> node)
    : node_(std::move(node)) {
    client_ = node_->create_client<Service>(
        "/operator/network/config/to_vehicle/set_actuation_enabled");
}

bool ActuationControlComponent::request(bool enable) {
    uint64_t request_id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_) {
            return false;
        }
        if (!client_->service_is_ready()) {
            last_error_ = "ACTUATION SERVICE UNAVAILABLE";
            return false;
        }
        pending_ = true;
        last_error_.clear();
        request_started_ = std::chrono::steady_clock::now();
        request_id = ++request_id_;
    }

    auto request = std::make_shared<Service::Request>();
    request->enable = enable;
    client_->async_send_request(
        request, [this, request_id](Client::SharedFuture future) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (request_id != request_id_) {
                return;
            }
            try {
                const auto response = future.get();
                if (!response->accepted) {
                    last_error_ = response->reason.empty()
                                      ? "ACTUATION REQUEST REJECTED"
                                      : response->reason;
                }
            } catch (const std::exception &error) {
                last_error_ = std::string("ACTUATION REQUEST FAILED: ") + error.what();
            }
            pending_ = false;
        });
    return true;
}

void ActuationControlComponent::update() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_ && std::chrono::steady_clock::now() - request_started_ >=
                        std::chrono::seconds(2)) {
        pending_ = false;
        ++request_id_;
        last_error_ = "ACTUATION REQUEST TIMEOUT";
    }
}

bool ActuationControlComponent::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_;
}

std::string ActuationControlComponent::last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

}  // namespace tod_gl

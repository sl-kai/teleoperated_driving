/**
* @file network_monitor_component.cpp
* @copyright 2025 TUMFTM
*/

#include "tod_gl/ros_interface/service_components/network_monitor_component.hpp"

using namespace std::chrono_literals;
 
namespace tod_gl {
 
NetworkMonitorComponent::NetworkMonitorComponent(std::shared_ptr<rclcpp::Node> subNode)
    : node_(subNode)  // Store subNode in node_ member variable
{
    local_client_ = node_->create_client<MonitorService>(
        "/operator/monitoring/network_monitor/set_monitoring_status");

    // Wait for service forwarder to start and get absolute name of set_monitoring_status
    rclcpp::sleep_for(100ms);

    auto services = subNode->get_service_names_and_types();
    std::string service_name = "/set_monitoring_status";

    for (const auto& service : services) {
        if (service.first.find("to_vehicle/set_monitoring_status") != std::string::npos) {
            service_name = service.first;
            break;
        }
    }

    // create client, if service was not found fall back to default name:
    // "/set_monitoring_status"
    vehicle_client_ = node_->create_client<MonitorService>(service_name);
    if (!local_client_->wait_for_service(1s)) {
        RCLCPP_WARN(node_->get_logger(),
                    "Service \"/operator/monitoring/network_monitor/set_monitoring_status\" not available");
    }
    if (!vehicle_client_->wait_for_service(1s)) {
        RCLCPP_WARN(node_->get_logger(), "Service \"%s\" not available", service_name.c_str());
    }
}

void NetworkMonitorComponent::SendRequest(
    const MonitorClient::SharedPtr &client,
    const std::string &target_ip,
    bool set_active,
    const std::string &endpoint_name) {
    if (!client || !client->service_is_ready()) {
        RCLCPP_WARN(node_->get_logger(),
                    "Latency monitor service unavailable for %s endpoint (target %s)",
                    endpoint_name.c_str(), target_ip.c_str());
        return;
    }

    auto request = std::make_shared<MonitorService::Request>();
    request->vehicle_ip_address = target_ip;
    request->set_monitor_mode = set_active;

    client->async_send_request(
        request,
        [this, set_active, endpoint_name](MonitorClient::SharedFuture future) {
            try {
                auto response = future.get();
                if (response->is_active != set_active) {
                    RCLCPP_ERROR(this->node_->get_logger(),
                                 "failed to change latency monitoring status on %s endpoint",
                                 endpoint_name.c_str());
                }
            } catch (const std::exception &e) {
                RCLCPP_ERROR(this->node_->get_logger(),
                             "latency monitoring request failed on %s endpoint: %s",
                             endpoint_name.c_str(), e.what());
            }
        });
}

void NetworkMonitorComponent::SetMonitorStatus(
    const std::string &operator_ip,
    const std::string &vehicle_ip,
    bool set_active) {
    SendRequest(local_client_, vehicle_ip, set_active, "operator");
    SendRequest(vehicle_client_, operator_ip, set_active, "vehicle");
}

} // namespace tod_gl

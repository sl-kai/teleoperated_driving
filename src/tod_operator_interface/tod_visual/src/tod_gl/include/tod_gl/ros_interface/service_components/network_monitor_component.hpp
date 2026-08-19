/**
 * @file network_monitor_component.hpp
 * @ingroup tod_gl_ros_interface
 **/
 #pragma once

 #include <rclcpp/rclcpp.hpp>
 
 #include "tod_network_monitoring_msgs/srv/network_monitor_service.hpp"
 
 namespace tod_gl {
 
 class NetworkMonitorComponent{
   public:
   NetworkMonitorComponent(std::shared_ptr<rclcpp::Node> subNode);
   void SetMonitorStatus(
       const std::string &operator_ip,
       const std::string &vehicle_ip,
       bool set_active);

   private:
   using MonitorService = tod_network_monitoring_msgs::srv::NetworkMonitorService;
   using MonitorClient = rclcpp::Client<MonitorService>;

   void SendRequest(
       const MonitorClient::SharedPtr &client,
       const std::string &target_ip,
       bool set_active,
       const std::string &endpoint_name);

   std::shared_ptr<rclcpp::Node> node_;
   MonitorClient::SharedPtr local_client_;
   MonitorClient::SharedPtr vehicle_client_;
   };
 
 } // namespace tod_gl

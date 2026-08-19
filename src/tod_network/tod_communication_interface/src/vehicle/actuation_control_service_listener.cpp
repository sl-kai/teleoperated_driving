#include <boost/asio.hpp>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <tod_network/tcp_receiver_boost.hpp>
#include <tod_network/tcp_sender_boost.hpp>
#include <tod_network/tod_service_listener.hpp>
#include <tod_network_protocols/udp_receiver.hpp>
#include <tod_network_protocols/udp_sender.hpp>
#include <tod_vehicle_msgs/srv/set_actuation_enabled.hpp>

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    if (argc < 5) {
        RCLCPP_ERROR(rclcpp::get_logger("ActuationControlServiceListener"),
                     "usage: <service> <protocol> <forwarder_port> <listener_port>");
        return 1;
    }

    using Listener = tod_network::ServiceListener<tod_vehicle_msgs::srv::SetActuationEnabled>;
    const std::string service_name = argv[1];
    const std::string protocol = argv[2];
    const int forwarder_port = std::stoi(argv[3]);
    const int listener_port = std::stoi(argv[4]);
    boost::asio::io_context io;
    std::shared_ptr<Listener> listener;

    if (protocol == "UDP") {
        listener = std::make_shared<Listener>(
            "ActuationControlServiceListener", service_name, true,
            std::make_unique<tod_network_protocols::UdpSender>(listener_port),
            std::make_unique<tod_network_protocols::UdpReceiver>(forwarder_port));
    } else {
        listener = std::make_shared<Listener>(
            "ActuationControlServiceListener", service_name, true,
            std::make_unique<tod_network::TcpSenderBoost>(io, listener_port),
            std::make_unique<tod_network::TcpReceiverBoost>(io, forwarder_port));
    }

    rclcpp::spin(listener);
    rclcpp::shutdown();
    return 0;
}

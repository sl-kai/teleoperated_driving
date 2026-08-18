#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

namespace {
constexpr std::array<canid_t, 5> kFeedbackIds{0x1A1, 0x1A2, 0x1A3, 0x1A4, 0x401};

bool is_feedback_frame(const can_frame &frame) {
  if ((frame.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0 || frame.len != 8) return false;
  for (const auto id : kFeedbackIds) if ((frame.can_id & CAN_SFF_MASK) == id) return true;
  return false;
}

std::string envelope(const can_frame &frame, int64_t stamp_ns, const std::string &interface) {
  std::ostringstream out;
  out << "{\"stamp_ns\":" << stamp_ns << ",\"interface\":\"" << interface
      << "\",\"id\":" << (frame.can_id & CAN_SFF_MASK) << ",\"id_hex\":\"0x"
      << std::uppercase << std::hex << (frame.can_id & CAN_SFF_MASK)
      << "\",\"is_extended\":false,\"is_rtr\":false,\"is_error\":false,\"dlc\":8,\"data_hex\":\"";
  for (uint8_t byte : frame.data) out << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
  return out.str() + "\"}";
}
}

class CanFeedbackRelay : public rclcpp::Node {
 public:
  CanFeedbackRelay() : Node("tod_can_feedback_relay") {
    interface_ = declare_parameter<std::string>("interface", "can0");
    publisher_ = create_publisher<std_msgs::msg::String>("/vehicle/can/raw", rclcpp::QoS(500).reliable());
    timer_ = create_wall_timer(std::chrono::milliseconds(10), [this] { receive_once(); });
  }
  ~CanFeedbackRelay() override { if (socket_ >= 0) close(socket_); }
 private:
  void receive_once() {
    if (socket_ < 0 && !open_socket()) return;
    pollfd descriptor{socket_, POLLIN, 0};
    if (poll(&descriptor, 1, 0) <= 0 || !(descriptor.revents & POLLIN)) return;
    while (true) {
      can_frame frame{};
      const auto received = recv(socket_, &frame, sizeof(frame), 0);
      if (received == sizeof(frame)) {
        if (!is_feedback_frame(frame)) continue;
        std_msgs::msg::String message;
        message.data = envelope(frame, get_clock()->now().nanoseconds(), interface_);
        publisher_->publish(message);
        continue;
      }
      if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
      if (received < 0 && errno == EINTR) continue;
      close(socket_); socket_ = -1; return;
    }
  }
  bool open_socket() {
    socket_ = socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, CAN_RAW);
    if (socket_ < 0) return false;
    std::array<can_filter, 5> filters{};
    for (size_t index = 0; index < kFeedbackIds.size(); ++index) { filters[index].can_id = kFeedbackIds[index]; filters[index].can_mask = CAN_SFF_MASK; }
    int receive_own_messages = 0;
    if (setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_FILTER, filters.data(), sizeof(filters)) != 0 || setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &receive_own_messages, sizeof(receive_own_messages)) != 0) { close(socket_); socket_ = -1; return false; }
    sockaddr_can address{}; address.can_family = AF_CAN; address.can_ifindex = static_cast<int>(if_nametoindex(interface_.c_str()));
    if (address.can_ifindex == 0 || bind(socket_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) { close(socket_); socket_ = -1; return false; }
    RCLCPP_INFO(get_logger(), "Reading SocketCAN feedback from %s", interface_.c_str());
    return true;
  }
  std::string interface_; int socket_{-1};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};
int main(int argc, char **argv) { rclcpp::init(argc, argv); rclcpp::spin(std::make_shared<CanFeedbackRelay>()); rclcpp::shutdown(); return 0; }

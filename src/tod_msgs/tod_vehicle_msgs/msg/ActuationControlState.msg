uint8 DISABLED=0
uint8 ARMING=1
uint8 ACTIVE=2
uint8 FAULT=3

std_msgs/Header header
uint8 state
bool enable_requested
string reason

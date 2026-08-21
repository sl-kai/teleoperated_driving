# TUM FTM Teleoperated Driving

本项目是 [TUMFTM/teleoperated_driving](https://github.com/TUMFTM/teleoperated_driving)
的 Fork，基于 Ubuntu 22.04、ROS 2 Humble 和 Docker，用于操作端与车端的远程驾驶实验。

![系统概览](doc/figures/visual_abstract.png)

## 本 Fork 的修改

- 支持 Logitech G923，并在操作端启动前自动设置回正力。
- 支持 Peanut01 的控制、车辆状态、三路相机视频和主雷达点云。
- 支持基于实测轮胎角和后轴运动学模型的前后轮轨迹显示。
- 网卡、ROS Domain ID、车辆 ID 和传感器 Domain ID 均可配置。

## 镜像

两个 GHCR 镜像均支持 `linux/amd64` 和 `linux/arm64`：

```text
ghcr.io/sl-kai/teleoperated-driving-operator:edge
ghcr.io/sl-kai/teleoperated-driving-vehicle:edge
```

```bash
git clone -b ros2 https://github.com/sl-kai/teleoperated_driving.git
cd teleoperated_driving
docker compose pull tod_operator tod_vehicle
```

如果 GHCR 连接不稳定，可以通过只读镜像源拉取后重新标记：

```bash
# 操作端
docker pull ghcr.nju.edu.cn/sl-kai/teleoperated-driving-operator:edge
docker tag ghcr.nju.edu.cn/sl-kai/teleoperated-driving-operator:edge \
  ghcr.io/sl-kai/teleoperated-driving-operator:edge

# 车端
docker pull ghcr.nju.edu.cn/sl-kai/teleoperated-driving-vehicle:edge
docker tag ghcr.nju.edu.cn/sl-kai/teleoperated-driving-vehicle:edge \
  ghcr.io/sl-kai/teleoperated-driving-vehicle:edge
```

## 配置

在两台机器上查看 IP 和网卡名称：

```bash
ip -br -4 addr
```

按实际环境修改 `.env`。两端的 `DOCKER_ROS_DOMAIN_ID` 和 `VEHICLE_ID` 必须一致，
监控网卡分别填写各自主机用于 TOD 通信的局域网接口：

```dotenv
DOCKER_ROS_DOMAIN_ID=7
VEHICLE_ID=peanut01
VEHICLE_NETWORK_INTERFACE=eth1
OPERATOR_NETWORK_INTERFACE=eno1
```

Peanut01 的视频、雷达、轨迹和车辆接口配置位于
`config/config/vehicle_config/peanut01/` 及 `config/config/package_config/`。
真实车辆测试前必须确认急停、安全驾驶员、控制限幅、挡位逻辑和车轮架空条件。

## 全新机器部署

在操作端宿主机安装并绑定支持 G923 的 Linux 方向盘驱动，例如兼容 G923 的 new-lg4ff。

在每台设备上安装 Docker Engine、Compose 插件和 Git，并确认版本：

```bash
docker version
docker compose version
```

拉取源码和部署配置：

```bash
git clone -b ros2 https://github.com/sl-kai/teleoperated_driving.git \
  ~/teleoperated_driving
cd ~/teleoperated_driving
```

根据设备修改 `.env` 中的 `DOCKER_ROS_DOMAIN_ID`、`VEHICLE_ID`、
`VEHICLE_NETWORK_INTERFACE` 和 `OPERATOR_NETWORK_INTERFACE`，然后按照上方
“镜像”章节拉取对应的操作端或车端镜像。

## 启动与停止

建议先启动车端，再启动操作端。

车端：

```bash
docker compose up -d tod_vehicle
docker compose ps
docker compose stop tod_vehicle
```

操作端：

```bash
docker compose up -d tod_operator
docker compose ps
docker compose stop tod_operator
```

在操作端 Manager 中填写车端 IP，选择本机操作端 IP、控制模式和输入配置，
然后依次点击 `Connect` 和 `Start`。没有方向盘时选择 `virtual.yaml`，
使用 G923 时选择 `logitechg923.yaml`。

也可以通过命令加载 G923 配置：

```bash
docker compose exec -T tod_operator bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 service call \
  /operator/input_devices/InputDevice/change_input_device \
  tod_operator_msgs/srv/InputDevice \
  "{input_device_directory: /home/tum/wsp/install/tod_input_devices/share/tod_input_devices/config/logitechg923.yaml}"
'
```

确认输入设备类型：

```bash
docker compose exec -T tod_operator bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 param get /operator/input_devices/InputDevice type
'
```

正确输出应为 `String value is: Usb`。查看 G923 发布的原始轴和按键数据：

```bash
docker compose exec -T tod_operator bash -lc '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic echo /operator/input_devices/output/joystick
'
```

数组下标从 `0` 开始，G923 的映射如下：

| 类型 | 下标 | 控件 |
| --- | ---: | --- |
| `axes` | 0 | 方向盘 |
| `axes` | 1 | 油门（最右侧） |
| `axes` | 2 | 刹车（中间） |
| `axes` | 3 | 离合（最左侧） |
| `buttons` | 0 | L3 |
| `buttons` | 1 | R3 |
| `buttons` | 2 | PS |
| `buttons` | 3 | 三条横线 |
| `buttons` | 4 | 回车 |
| `buttons` | 5 | + |
| `buttons` | 6 | - |
| `buttons` | 7 | 右拨片 |
| `buttons` | 8 | 左拨片 |
| `buttons` | 9 | O |
| `buttons` | 10 | X |
| `buttons` | 11 | 方形 |
| `buttons` | 12 | 三角形 |

查看发布频率：

```bash
docker compose exec -T tod_operator bash -lc '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic hz /operator/input_devices/output/joystick
'
```

## 挡位与换挡

操作端支持三个挡位：

| 挡位 | 数值 | 含义 |
| --- | ---: | --- |
| R | 1 | 倒挡 |
| N | 2 | 空挡，也是默认挡位 |
| D | 3 | 前进挡 |

G923 使用两个拨片执行升挡和降挡。只有车辆接近静止时才允许换挡。

## 更新部署

拉取并标记新镜像后，只重建对应服务：

```bash
# 操作端
docker compose up -d --no-deps --force-recreate tod_operator

# 车端
docker compose up -d --no-deps --force-recreate tod_vehicle
```

## 常用检查

以下命令需要在对应的项目目录中执行。查看操作端 ROS 图：

```bash
docker compose exec -T tod_operator bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 node list
ros2 topic list
'
```

车端将服务名改为 `tod_vehicle`。查看任意话题的数据或频率：

```bash
docker compose exec -T <SERVICE> bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic echo <TOPIC>
'
```

例如：

```bash
docker compose exec -T tod_vehicle bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic hz /vehicle/network/data/from_operator/primary_control_cmd
'
```

常用话题：

| 用途 | 服务 | 话题 |
| --- | --- | --- |
| G923 原始输入 | `tod_operator` | `/operator/input_devices/output/joystick` |
| 操作端主控制命令 | `tod_operator` | `/operator/network/data/to_vehicle/primary_control_cmd` |
| 操作端挡位等命令 | `tod_operator` | `/operator/network/data/to_vehicle/secondary_control_cmd` |
| 车端收到的主控制命令 | `tod_vehicle` | `/vehicle/network/data/from_operator/primary_control_cmd` |
| 车端收到的挡位等命令 | `tod_vehicle` | `/vehicle/network/data/from_operator/secondary_control_cmd` |
| 左前轨迹 | `tod_operator` | `/operator/interface/visual/input/driving_lane_front_left` |

正常情况下控制命令频率约为 `10 Hz`。使用 `Ctrl+C` 退出持续输出。

检查车端安全模块和 Peanut01 接口输出：

```bash
docker compose exec -T tod_vehicle bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic echo /vehicle/safety/output/primary_control_cmd
'

docker compose exec -T tod_vehicle bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic echo /debug/tod_peanut01/autoware_control_cmd
'
```

## 启动控制

默认为 `DISABLED` 关闭状态。启用前必须确认车辆处于软件 N 挡、踏板完全松开、车辆周围无人。启动后进入 `ARMING` 等待安全条件；在 N 挡等待一秒且反馈信息正常后，进入 `ACTIVE` 激活状态。故障进入 `FAULT` 后，先关闭真实控制并排除急停、许可、命令超时、反馈超时、本地接管和底盘执行反馈不一致等原因，再重新启用。

启动控制（或长按左下角按钮）：

```bash
docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=7 && ros2 param set /vehicle/interface/peanut01/ControlBridge enable_actuation true"
```

关闭控制（或点按左下角按钮）：

```bash
docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=7 && ros2 param set /vehicle/interface/peanut01/ControlBridge enable_actuation false"
```

查看启用参数和诊断状态：

```bash
docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=7 && ros2 param get /vehicle/interface/peanut01/ControlBridge enable_actuation"

docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=0 && ros2 topic echo /debug/tod_peanut01/bridge_diagnostics --once"
```

状态含义：`DISABLED` 为关闭，`ARMING` 为等待安全条件，`ACTIVE` 为已激活，
`FAULT` 为安全故障锁存。

## 上游与许可证

本 Fork 保留上游版权和 [GNU LGPL v3](LICENSE) 许可证。

论文：T. Kerbl et al., *TUM Teleoperation: Open Source Software for Remote Driving
and Assistance of Automated Vehicles*, 2025,
[doi:10.48550/arXiv.2506.13933](https://doi.org/10.48550/arXiv.2506.13933)。

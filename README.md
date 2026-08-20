# TUM FTM Teleoperated Driving

本项目是 [TUMFTM/teleoperated_driving](https://github.com/TUMFTM/teleoperated_driving)
的 Fork，基于 Ubuntu 22.04、ROS 2 Humble 和 Docker，用于操作端与车端的远程驾驶实验。

![系统概览](doc/figures/visual_abstract.png)

## 本 Fork 的修改

- 支持 Logitech G923，并在操作端启动前自动设置 80% 回正力。
- 支持 Peanut01 的控制、车辆状态、三路相机视频和主雷达点云。
- 支持基于实测轮胎角和后轴运动学模型的前后轮轨迹显示。
- 网卡、ROS Domain ID、车辆 ID 和传感器 Domain ID 均可配置。
- 操作端和车端程序已构建进正式镜像，不依赖源码、work 或 overlay 挂载。

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
docker compose logs --tail 100 tod_vehicle
docker compose stop tod_vehicle
```

操作端：

```bash
docker compose up -d tod_operator
docker compose ps
docker compose logs --tail 100 tod_operator
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

正确输出应为 `String value is: Usb`。查看方向盘原始数据和发布频率：

```bash
docker compose exec -T tod_operator bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic echo /operator/input_devices/output/joystick
'

docker compose exec -T tod_operator bash -c '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
ros2 topic hz /operator/input_devices/output/joystick
'
```

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

`DryRunInterface` 的调试输出不代表命令已经发送到底盘。只有真实控制桥处于
`ACTIVE`，并且真实控制话题存在发布者时，才会向底盘接口发布命令。

## 真实控制安全检查

默认情况下真实控制关闭。启用前必须确认车辆处于软件 N 挡、踏板完全松开、
车辆周围无人且急停可用：

```bash
docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=7 && ros2 param set /vehicle/interface/peanut01/ControlBridge enable_actuation true"
```

关闭真实控制：

```bash
docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=7 && ros2 param set /vehicle/interface/peanut01/ControlBridge enable_actuation false"
```

查看启用参数和诊断状态：

```bash
docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=7 && ros2 param get /vehicle/interface/peanut01/ControlBridge enable_actuation"

docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=0 && ros2 topic echo /debug/tod_peanut01/bridge_diagnostics --once"
```

状态含义：`DISABLED` 为关闭，`ARMING` 为等待安全条件，`ACTIVE` 为已激活，
`FAULT` 为安全故障锁存。`ACTIVE` 时以下四个真实控制话题的发布者数量应为 `1`；
`DISABLED` 或 `FAULT` 时应为 `0`：

```bash
docker compose exec -T tod_vehicle bash -lc '
source /opt/ros/humble/setup.bash
source /home/tum/wsp/install/setup.bash
export ROS_DOMAIN_ID=0
ros2 topic info /control/command/control_cmd
ros2 topic info /control/command/gear_cmd
ros2 topic info /control/command/turn_indicators_cmd
ros2 topic info /control/command/hazard_lights_cmd
'
```

故障进入 `FAULT` 后，先关闭真实控制并排除急停、许可、命令超时、反馈超时、
本地接管和底盘执行反馈不一致等原因，再重新启用：

```bash
docker compose exec -T tod_vehicle bash -lc "source /opt/ros/humble/setup.bash && source /home/tum/wsp/install/setup.bash && export ROS_DOMAIN_ID=7 && ros2 param set /vehicle/interface/peanut01/ControlBridge enable_actuation false"
```

## 构建与发布

需要从源码本地构建时：

```bash
./setup_repos.sh
docker compose build tod_vehicle tod_operator
```

推送 `ros2` 分支会发布 `edge`。推送 `v1.0.0` 格式的 Git 标签会发布
`1.0.0` 和 `latest`：

```bash
git tag v1.0.0
git push origin v1.0.0
```

## 上游与许可证

本 Fork 保留上游版权和 [GNU LGPL v3](LICENSE) 许可证。

论文：T. Kerbl et al., *TUM Teleoperation: Open Source Software for Remote Driving
and Assistance of Automated Vehicles*, 2025,
[doi:10.48550/arXiv.2506.13933](https://doi.org/10.48550/arXiv.2506.13933)。

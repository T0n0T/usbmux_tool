# CH32V208 Host 用户态驱动说明

## 目标

`Host/linux/userspace` 提供一个先于内核态驱动落地的用户态驱动实现，用于验证当前固件的 USB Vendor 协议、端点收发和 UART/SYS 通道语义。

该实现不向系统注册 tty 设备。它的职责是把协议和传输路径跑通，并为后续内核态驱动沉淀稳定的协议边界。

## 与固件的接口约定

- `VID:PID`：`1A86:2080`
- USB Interface：`0`
- `EP3 OUT (0x03)`：发送完整私有协议帧
- `EP2 IN (0x82)`：读取完整私有协议帧
- `EP1 IN (0x81)`：读取 `8B` 异步提示包
- 最大帧长：`512B`
- 帧头长度：`24B`
- 协议版本：`0x01`

帧头字段、小端序布局、CRC16 算法与固件 `App/usb_mux_dev/proto/vendor_proto_codec.c` 保持一致。

## 模块边界

- `userspace/include/ch32v208_mux/proto.h` 与 `userspace/src/proto.c`
  - 协议常量
  - 帧头编码、解码
  - `CRC16-CCITT` 计算
  - `EP1` hint 解析
- `userspace/include/ch32v208_mux/device.h` 与 `userspace/src/device.c`
  - libusb 初始化
  - 设备打开与接口 claim
  - `EP3 OUT` / `EP2 IN` bulk 帧收发
  - `EP1` interrupt hint 读取
- `userspace/include/ch32v208_mux/uart.h` 与 `userspace/src/uart.c`
  - `SYS_GET_DEV_INFO`
  - `SYS_GET_CAPS`
  - `SYS_HEARTBEAT`
  - UART port capability/open/close/stats
  - UART data write/read
- `userspace/tools/ch32v208-mux-cli.c`
  - 面向现场验证的命令行入口

## 构建与测试

```sh
cmake -S Host/linux/userspace -B Host/userspace/build
cmake --build Host/userspace/build
ctest --test-dir Host/userspace/build --output-on-failure
```

协议单元测试不依赖硬件，覆盖：

- 帧头编码与解码
- 帧头 CRC 错误检测
- 最大 payload 边界
- `EP1` hint 解析

## CLI 使用

```sh
Host/userspace/build/ch32v208-mux-cli probe
Host/userspace/build/ch32v208-mux-cli heartbeat ping
Host/userspace/build/ch32v208-mux-cli uart-cap 0
Host/userspace/build/ch32v208-mux-cli uart-open 0 115200
Host/userspace/build/ch32v208-mux-cli uart-close 0
Host/userspace/build/ch32v208-mux-cli debug-xfer
```

`debug-xfer` 只发送一个 `SYS_GET_DEV_INFO` 帧，并打印 EP3 OUT / EP2 IN 的 libusb 返回值、实际传输字节数和原始帧内容，用于定位 Host 与固件端点收发问题。

如果返回 `ERR_NOT_FOUND`，优先确认：

1. 固件已经烧录并完成 USB 枚举。
2. 当前用户有权限访问 `1A86:2080`。
3. 虚拟机环境中 USB 设备已经直通到当前系统。

## udev 权限规则

普通用户访问当前固件 USB 设备时，建议安装本项目提供的规则：

```sh
sudo install -m 0644 Host/userspace/udev/60-ch32v208-mux.rules /etc/udev/rules.d/60-ch32v208-mux.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

规则内容：

```udev
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", ATTR{idProduct}=="2080", MODE="0660", GROUP="uucp", TAG+="uaccess"
```

说明：

- `GROUP="uucp"`：允许 `uucp` 组成员直接访问该 USB 设备。
- `TAG+="uaccess"`：在使用 systemd-logind 的桌面/本地会话中，授予当前活跃用户访问权限。
- `MODE="0660"`：不向所有用户开放设备节点。

当前用户 `Tiger` 已在 `uucp` 组内，因此安装规则并重新触发 udev 后通常不需要再修改组成员。如果现场机器用户不在 `uucp` 组，可执行：

```sh
sudo usermod -aG uucp "$USER"
```

然后重新登录。

## 后续迁移到内核态

迁移时建议保持三层边界：

1. 协议层
   - 复用 `proto` 的布局、CRC 和状态码。
   - 避免把 USB URB 细节混入协议编解码。
2. USB 传输层
   - 用 probe/remove 管理接口生命周期。
   - 用 bulk IN/OUT URB 替代 libusb bulk transfer。
   - 用 interrupt IN URB 订阅 hint。
3. tty 暴露层
   - 将 `UART_DATA` 通道接入 tty flip buffer。
   - 将 open/close/termios 映射到当前 UART 控制命令。
   - 将设备状态码转换为 Linux errno。

当前用户态实现只覆盖第一步和第二步的行为验证，不提前引入 tty/pty 模拟层。

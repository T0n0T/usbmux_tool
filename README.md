# CH32V208 Mux Host

这是当前固件的 Host 侧用户态驱动子项目，使用 `libusb-1.0` 直接对接 `CH32V208 USB Vendor UART + BLE Host` 设备。

当前边界：

- USB 设备：`VID:PID = 1A86:2080`
- Interface：`0`
- `EP2 OUT 0x02`：Host 到 Device 私有协议帧
- `EP2 IN 0x82`：Device 到 Host 私有协议帧
- `EP1 IN 0x81`：8 字节异步提示包
- 协议头与固件 `App/usb_mux_dev/proto/vendor_proto.h` 对齐

## 构建

依赖：

- CMake 3.16+
- C 编译器
- `pkg-config`
- `libusb-1.0`

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## CLI

```sh
./build/ch32v208-mux-cli probe
./build/ch32v208-mux-cli heartbeat ping
./build/ch32v208-mux-cli uart-cap 0
./build/ch32v208-mux-cli uart-open 0 115200
./build/ch32v208-mux-cli uart-close 0
```

Linux 下普通用户访问 USB 设备可能需要 udev 规则或临时使用 root 权限。

## udev 权限

当前仓库提供规则文件：

```sh
sudo install -m 0644 udev/60-ch32v208-mux.rules /etc/udev/rules.d/60-ch32v208-mux.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

规则将 `1A86:2080` 授权给 `uucp` 组，并添加 `uaccess` 标签。当前用户需要在 `uucp` 组内；修改组成员后通常需要重新登录。

## 内核态迁移边界

`src/proto.c` 只处理协议编码、解码、CRC 和 hint 解析，迁移到内核态时应作为首要复用对象。`src/device.c` 是 libusb 传输层，迁移时替换为 USB core、URB 和 interface probe/remove。`src/uart.c` 体现当前 SYS/UART 通道语义，后续内核态版本应将 UART 通道接入 tty core。

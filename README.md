# CH32V208 Mux — Host 侧实现

本子仓库涵盖 CH32V208 USB Vendor Mux 设备的 Host 侧软件，以 Linux 为主要目标平台。

```
Host/
├── .git
├── README.md              本文件
├── docs/                  架构与设计文档
│   └── USERSPACE_DRIVER.md
├── userspace/             用户态驱动（libusb，可用）
│   ├── CMakeLists.txt
│   ├── include/ch32v208_mux/   (proto.h, device.h, uart.h, ble.h)
│   ├── src/                    (proto.c, device.c, uart.c, ble.c)
│   ├── tools/ch32v208-mux-cli.c
│   ├── tests/test_proto.c
│   ├── udev/                   USB 设备权限规则
│   └── README.md
└── module/                内核态驱动（规划中）
    └── REUSE_ASSESSMENT.md
```

| 目录 | 状态 | 说明 |
|------|------|------|
| `userspace/` | 可用 | 基于 libusb 的验证层，跑通协议和 USB 传输路径 |
| `module/` | 规划中 | 内核 USB 驱动，通过 tty core 暴露 UART 通道 |

## 构建（用户态）

```sh
cmake -S Host/userspace -B Host/userspace/build
cmake --build Host/userspace/build
ctest --test-dir Host/userspace/build --output-on-failure
```

## 硬件接口

- **VID:PID** — `1A86:2080`
- **EP3 OUT (0x03)** — 发送私有协议帧
- **EP2 IN (0x82)** — 读取私有协议帧
- **EP1 IN (0x81)** — 8 字节异步提示包
- 帧头 24B、小端序、CRC16-CCITT，与固件 `App/usb_mux_dev/proto/` 对齐

详情见 [docs/USERSPACE_DRIVER.md](docs/USERSPACE_DRIVER.md)。

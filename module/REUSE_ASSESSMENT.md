# 内核模块复用评估

评估基准：`Host/linux/userspace/` 当前用户态实现。

## 层次复用分析

```
┌──────────────────────────────────────────────────────────────┐
│  userspace                    │  module (kernel)             │
├───────────────────────────────┼──────────────────────────────┤
│  device.h / device.c         │  ───▶  完全重写               │
│   (libusb 传输层)            │          USB core API / URB   │
├───────────────────────────────┼──────────────────────────────┤
│  uart.c / ble.c              │  ───▶  部分复用               │
│   (协议语义/命令编排)         │          命令填充/解析逻辑保留  │
│                              │          替换 transport 为 URB │
├───────────────────────────────┼──────────────────────────────┤
│  proto.h / proto.c           │  ───▶  直接复用               │
│   (协议编解码/CRC/常量)       │          适配 kernel types     │
├───────────────────────────────┼──────────────────────────────┤
│  cli.c / test_proto.c        │  ───▶  不涉及                  │
│   (用户态入口与测试)          │                                │
└───────────────────────────────┴──────────────────────────────┘
```

---

## 第 1 层：协议核心 (proto.h / proto.c) — 直接复用

**依赖**: 仅 `<stdint.h>` `<stddef.h>` `<string.h>`，无 OS 调用。

| 函数/定义 | 复用方式 |
|-----------|----------|
| `ch32mux_crc16()`       | 直接复制，`stdint.h` → `linux/types.h`，`u16`/`u8` 兼容 |
| `ch32mux_encode_header()` | 同上 |
| `ch32mux_decode_header()` | 同上 |
| `ch32mux_build_frame()`   | 同上 |
| `ch32mux_parse_irq_hint()` | 同上 |
| `ch32mux_result_name()` / `ch32mux_device_status_name()` | 可选，debugfs 用 |
| 所有 `ch32mux_*` 枚举和常量 | `#include` 直接引用 |
| `ch32mux_header_t` / `ch32mux_dev_info_t` / `ch32mux_caps_t` 等 | 结构体布局直接复用 |

**改动点**:
- `#include <stdint.h>` → `#include <linux/types.h>` (提供 `u8`/`u16`/`__le16`)
- `size_t` → `size_t` 在 kernel 中也是 `__kernel_size_t`，可直接用
- CRC 算法不变，但可考虑查表法优化性能
- 错误码 `CH32MUX_ERR_*` 需要与内核 `errno.h` 映射（或保持独立枚举）

**建议**: 提取为 `linux/module/proto/` 共享头文件 + 实现，用户态和内核态共用同一份源文件。

---

## 第 2 层：USB 传输层 (device.h / device.c) — 完全重写

用户态基于 libusb，内核态使用 USB core API，差异巨大：

| 用户态 (libusb) | 内核态 (USB core) |
|-----------------|-------------------|
| `libusb_init()` / `libusb_exit()` | `usb_register()` / `usb_deregister()` — 模块 probe/remove |
| `libusb_open_device_with_vid_pid()` | `usb_match_id()` — probe 时匹配 |
| `libusb_detach_kernel_driver()` | 不需要 — 自己就是内核驱动 |
| `libusb_claim_interface()` | probe 中自动 claim |
| `libusb_bulk_transfer()` | `usb_bulk_msg()` 或提交 URB |
| `libusb_interrupt_transfer()` | 提交 interrupt URB |
| `calloc()` / `free()` | `kzalloc()` / `kfree()` |
| `ch32mux_device_t` (opaque ptr) | `struct usb_device *` + 自定义结构体 |
| `next_seq`, `timeout_ms` 访问器 | 保留为内联函数或直接访问字段 |

**可保留的逻辑模式**:
- `ch32mux_write_frame()` 的 EP3 OUT 分块发送策略
- `ch32mux_read_frame()` 的"先读 header → 解码 → 再读 body"两阶段策略
- `map_libusb_error()` → 映射为内核 `errno` ( `-ETIMEDOUT` / `-ENODEV` / `-EIO` )

---

## 第 3 层：协议语义 (uart.c / ble.c) — 部分复用

### transact() 模式

uart.c 和 ble.c 各自有 `transact()` 内部函数。两者几乎相同：
```
build_frame → write_frame → read_frame → decode_header → validate → copy_payload
```

**复用建议**: 提取为模块级的公共辅助函数 `ch32mux_transact()`，将 `write_frame` / `read_frame` 替换为 URB 操作。

### 命令函数复用性

| 函数 | 评估 |
|------|------|
| `ch32mux_sys_get_dev_info()` | 命令构建 + payload 解析逻辑可复用 |
| `ch32mux_sys_get_caps()` | 同上 |
| `ch32mux_sys_heartbeat()` | 纯透传，可复用 |
| `ch32mux_uart_get_port_cap()` | payload 解析 `read_le32` 可复用（`get_unaligned_le32`）|
| `ch32mux_uart_open()` | build payload 逻辑可复用 |
| `ch32mux_uart_close()` | 零 payload 命令，可复用 |
| `ch32mux_uart_get_stats()` | payload 解析可复用 |
| `ch32mux_uart_write()` | **概念保留** — 但数据需经 tty 层 flip buffer，数据流方向不同 |
| `ch32mux_uart_read()` | **概念保留** — 需通过 URB 完成回调 + tty flip buffer 推数据 |
| `ch32mux_ble_get_cap()` | payload 解析可复用 |
| `ch32mux_ble_set_scan_param()` | payload 构建可复用 |
| `ch32mux_ble_scan_start/stop()` | 零 payload 命令，可复用 |
| `ch32mux_ble_parse_scan_result()` | **直接可复用** — 纯解析，无 IO |
| `ch32mux_ble_read_event()` | 模式可复用，但需 URB 替代 `read_frame` |

### 头文件结构体

`uart.h` 和 `ble.h` 中定义的结构体:
- `ch32mux_uart_line_coding_t`
- `ch32mux_uart_port_cap_t`
- `ch32mux_uart_stats_t`
- `ch32mux_ble_cap_t`
- `ch32mux_ble_scan_result_t`

全部可直接搬移到共用头文件，布局与固件侧一致。

---

## 第 4 层：入口与测试 (cli.c / test_proto.c) — 不涉及

- **CLI**: 保留在 userspace/，模块不需要。
- **单元测试**: 协议层测试（`test_proto.c`）可在 kernel 中用 kunit 重写。

---

## 目录结构建议

```
linux/module/
├── include/
│   └── ch32v208_mux/
│       ├── proto.h          ← 与 userspace 共用（#ifdef __KERNEL__ 适配）
│       ├── device.h         ← 新写，kernel USB API
│       ├── uart.h           ← 结构体共用，API 面向 tty 层
│       └── ble.h            ← 结构体共用
├── proto/
│   └── proto.c              ← 与 userspace 共用源文件（CFLAGS 区分）
├── ch32v208-mux-core.c      ← USB probe/remove, URB 回调
├── ch32v208-mux-uart.c      ← tty 驱动入口 + uart transact
├── ch32v208-mux-ble.c       ← BLE 命令 + scan event 处理
└── Kbuild / Kconfig
```

---

## 推荐实现顺序

1. **共用头文件** — proto.h 添加 `#ifdef __KERNEL__` 路径，结构体集中管理
2. **协议层** — proto.c 编译进模块，验证 CRC/编解码正确性
3. **USB 核心** — probe/remove、URB submit + 完成回调、hint 中断处理
4. **tty 驱动** — 注册 tty_driver，将 UART_DATA 通道接入 flip buffer
5. **SYS 通道** — get_dev_info / get_caps / heartbeat 命令
6. **UART 控制** — open/close/termios 映射
7. **调试接口** — debugfs 暴露原始帧交换

---

## 量化总结

| 模块 | 用户态代码行 | 内核态可复用行 | 复用率 |
|------|-------------|---------------|--------|
| proto.h | 186 | ~180 | **~97%** |
| proto.c | 336 | ~310 | **~92%** |
| device.h | 49 | ~10 | **~20%** |
| device.c | 341 | ~50 | **~15%** |
| uart.h | 69 | ~50 | **~72%** |
| uart.c | 416 | ~200 | **~48%** |
| ble.h | 56 | ~40 | **~71%** |
| ble.c | 239 | ~120 | **~50%** |
| **总计** | **~1692** | **~960** | **~57%** |

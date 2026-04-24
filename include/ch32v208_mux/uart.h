#ifndef CH32V208_MUX_UART_H
#define CH32V208_MUX_UART_H

#include <stddef.h>
#include <stdint.h>

#include "ch32v208_mux/device.h"
#include "ch32v208_mux/proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t baudrate;
    uint8_t data_bits;
    uint8_t parity;
    uint8_t stop_bits;
} ch32mux_uart_line_coding_t;

typedef struct {
    uint8_t logic_port;
    uint8_t phy_uart_id;
    uint8_t state;
    uint8_t reserved0;
    uint16_t rx_capacity;
    uint16_t tx_capacity;
    uint8_t supports_line_coding;
    uint8_t supports_flush;
    uint8_t reserved1;
    uint8_t reserved2;
} ch32mux_uart_port_cap_t;

typedef struct {
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t drop_rx_bytes;
    uint32_t drop_tx_bytes;
    uint8_t state;
    uint8_t data_bits;
    uint8_t parity;
    uint8_t stop_bits;
} ch32mux_uart_stats_t;

int ch32mux_sys_get_dev_info(ch32mux_device_t *device, ch32mux_dev_info_t *info);
int ch32mux_sys_get_caps(ch32mux_device_t *device, ch32mux_caps_t *caps);
int ch32mux_sys_heartbeat(ch32mux_device_t *device,
                          const uint8_t *payload,
                          size_t payload_len,
                          uint8_t *response,
                          size_t response_capacity,
                          size_t *response_len);
int ch32mux_uart_get_port_cap(ch32mux_device_t *device, uint8_t port, ch32mux_uart_port_cap_t *cap);
int ch32mux_uart_open(ch32mux_device_t *device, uint8_t port, const ch32mux_uart_line_coding_t *line);
int ch32mux_uart_close(ch32mux_device_t *device, uint8_t port);
int ch32mux_uart_get_stats(ch32mux_device_t *device, uint8_t port, ch32mux_uart_stats_t *stats);
int ch32mux_uart_write(ch32mux_device_t *device, uint8_t port, const uint8_t *data, size_t len);
int ch32mux_uart_read(ch32mux_device_t *device,
                      uint8_t port,
                      uint8_t *data,
                      size_t capacity,
                      size_t *len);

#ifdef __cplusplus
}
#endif

#endif

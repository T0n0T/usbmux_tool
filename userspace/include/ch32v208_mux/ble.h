#ifndef CH32V208_MUX_BLE_H
#define CH32V208_MUX_BLE_H

#include <stddef.h>
#include <stdint.h>

#include "ch32v208_mux/device.h"
#include "ch32v208_mux/proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t max_links;
    uint8_t supports_scan;
    uint8_t supports_connect;
    uint8_t supports_gatt;
    uint16_t scan_interval;
    uint16_t scan_window;
} ch32mux_ble_cap_t;

typedef struct {
    uint8_t addr_type;
    uint8_t event_type;
    int8_t rssi;
    uint8_t adv_len;
    uint8_t addr[6];
    uint8_t adv_prefix[6];
} ch32mux_ble_scan_result_t;

int ch32mux_ble_get_cap(ch32mux_device_t *device, ch32mux_ble_cap_t *cap);
int ch32mux_ble_set_scan_param(ch32mux_device_t *device,
                               uint16_t interval,
                               uint16_t window,
                               uint16_t duration,
                               uint8_t mode,
                               uint8_t active,
                               uint8_t white_list);
int ch32mux_ble_scan_start(ch32mux_device_t *device);
int ch32mux_ble_scan_stop(ch32mux_device_t *device);
int ch32mux_ble_parse_scan_result(const uint8_t *payload,
                                  size_t payload_len,
                                  ch32mux_ble_scan_result_t *result);
int ch32mux_ble_read_event(ch32mux_device_t *device,
                           ch32mux_header_t *header,
                           uint8_t *payload,
                           size_t payload_capacity,
                           size_t *payload_len);

#ifdef __cplusplus
}
#endif

#endif

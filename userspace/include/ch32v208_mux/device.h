#ifndef CH32V208_MUX_DEVICE_H
#define CH32V208_MUX_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include <libusb-1.0/libusb.h>

#include "ch32v208_mux/proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ch32mux_device ch32mux_device_t;

typedef struct {
    uint16_t vid;
    uint16_t pid;
    int interface_number;
    unsigned int timeout_ms;
    int detach_kernel_driver;
} ch32mux_open_options_t;

void ch32mux_default_open_options(ch32mux_open_options_t *options);
int ch32mux_open(const ch32mux_open_options_t *options, ch32mux_device_t **device_out);
void ch32mux_close(ch32mux_device_t *device);
int ch32mux_write_frame(ch32mux_device_t *device, const uint8_t *frame, size_t frame_len);
int ch32mux_read_frame(ch32mux_device_t *device,
                       uint8_t *frame,
                       size_t frame_capacity,
                       size_t *frame_len);
int ch32mux_read_hint(ch32mux_device_t *device, ch32mux_irq_hint_t *hint);
int ch32mux_debug_bulk_transfer(ch32mux_device_t *device,
                                uint8_t endpoint,
                                uint8_t *data,
                                int length,
                                int *transferred,
                                unsigned int timeout_ms,
                                int *libusb_status);
uint16_t ch32mux_next_seq(ch32mux_device_t *device);
unsigned int ch32mux_timeout_ms(const ch32mux_device_t *device);

#ifdef __cplusplus
}
#endif

#endif

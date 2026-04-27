#include "ch32v208_mux/device.h"

#include <stdlib.h>
#include <string.h>

struct ch32mux_device {
    libusb_context *ctx;
    libusb_device_handle *handle;
    int interface_number;
    int claimed;
    int detached_kernel_driver;
    unsigned int timeout_ms;
    uint16_t next_seq;
};

static int map_libusb_error(int err)
{
    if(err == LIBUSB_ERROR_TIMEOUT)
    {
        return CH32MUX_ERR_TIMEOUT;
    }
    if(err == LIBUSB_ERROR_NO_DEVICE)
    {
        return CH32MUX_ERR_NOT_FOUND;
    }
    return CH32MUX_ERR_IO;
}

void ch32mux_default_open_options(ch32mux_open_options_t *options)
{
    if(options == NULL)
    {
        return;
    }

    options->vid = CH32MUX_USB_VID;
    options->pid = CH32MUX_USB_PID;
    options->interface_number = CH32MUX_USB_INTERFACE;
    options->timeout_ms = 1000;
    options->detach_kernel_driver = 1;
}

int ch32mux_open(const ch32mux_open_options_t *options, ch32mux_device_t **device_out)
{
    ch32mux_open_options_t local_options;
    ch32mux_device_t *device;
    int ret;

    if(device_out == NULL)
    {
        return CH32MUX_ERR_ARG;
    }
    *device_out = NULL;

    if(options == NULL)
    {
        ch32mux_default_open_options(&local_options);
        options = &local_options;
    }

    device = (ch32mux_device_t *)calloc(1, sizeof(*device));
    if(device == NULL)
    {
        return CH32MUX_ERR_NO_MEM;
    }

    ret = libusb_init(&device->ctx);
    if(ret != LIBUSB_SUCCESS)
    {
        free(device);
        return map_libusb_error(ret);
    }

    device->handle = libusb_open_device_with_vid_pid(device->ctx, options->vid, options->pid);
    if(device->handle == NULL)
    {
        ch32mux_close(device);
        return CH32MUX_ERR_NOT_FOUND;
    }

    device->interface_number = options->interface_number;
    device->timeout_ms = options->timeout_ms;
    device->next_seq = 1;

    if(options->detach_kernel_driver &&
       libusb_kernel_driver_active(device->handle, device->interface_number) == 1)
    {
        ret = libusb_detach_kernel_driver(device->handle, device->interface_number);
        if(ret != LIBUSB_SUCCESS)
        {
            ch32mux_close(device);
            return map_libusb_error(ret);
        }
        device->detached_kernel_driver = 1;
    }

    ret = libusb_claim_interface(device->handle, device->interface_number);
    if(ret != LIBUSB_SUCCESS)
    {
        ch32mux_close(device);
        return map_libusb_error(ret);
    }

    device->claimed = 1;
    *device_out = device;
    return CH32MUX_OK;
}

void ch32mux_close(ch32mux_device_t *device)
{
    if(device == NULL)
    {
        return;
    }

    if((device->handle != NULL) && device->claimed)
    {
        (void)libusb_release_interface(device->handle, device->interface_number);
    }

    if((device->handle != NULL) && device->detached_kernel_driver)
    {
        (void)libusb_attach_kernel_driver(device->handle, device->interface_number);
    }

    if(device->handle != NULL)
    {
        libusb_close(device->handle);
    }

    if(device->ctx != NULL)
    {
        libusb_exit(device->ctx);
    }

    free(device);
}

int ch32mux_write_frame(ch32mux_device_t *device, const uint8_t *frame, size_t frame_len)
{
    size_t offset = 0;

    if((device == NULL) || (frame == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if((frame_len == 0U) || (frame_len > CH32MUX_MAX_FRAME_LEN))
    {
        return CH32MUX_ERR_OVERFLOW;
    }

    while(offset < frame_len)
    {
        int transferred = 0;
        int chunk = (int)(frame_len - offset);
        int ret;

        ret = libusb_bulk_transfer(device->handle,
                                   CH32MUX_EP_FRAME_OUT,
                                   (unsigned char *)&frame[offset],
                                   chunk,
                                   &transferred,
                                   device->timeout_ms);
        if(ret != LIBUSB_SUCCESS)
        {
            return map_libusb_error(ret);
        }
        if(transferred <= 0)
        {
            return CH32MUX_ERR_IO;
        }
        offset += (size_t)transferred;
    }

    return CH32MUX_OK;
}

int ch32mux_read_frame(ch32mux_device_t *device,
                       uint8_t *frame,
                       size_t frame_capacity,
                       size_t *frame_len)
{
    size_t offset = 0;
    size_t target_len = CH32MUX_HEADER_LEN;
    ch32mux_header_t header;

    if((device == NULL) || (frame == NULL) || (frame_len == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if(frame_capacity < CH32MUX_HEADER_LEN)
    {
        return CH32MUX_ERR_OVERFLOW;
    }

    *frame_len = 0;

    while(offset < target_len)
    {
        int transferred = 0;
        int capacity = (int)(frame_capacity - offset);
        int ret;

        if(capacity <= 0)
        {
            return CH32MUX_ERR_OVERFLOW;
        }
        if(capacity > (int)CH32MUX_EP_PACKET_SIZE)
        {
            capacity = (int)CH32MUX_EP_PACKET_SIZE;
        }

        ret = libusb_bulk_transfer(device->handle,
                                   CH32MUX_EP_FRAME_IN,
                                   &frame[offset],
                                   capacity,
                                   &transferred,
                                   device->timeout_ms);
        if(ret != LIBUSB_SUCCESS)
        {
            return map_libusb_error(ret);
        }
        if(transferred <= 0)
        {
            return CH32MUX_ERR_IO;
        }

        offset += (size_t)transferred;

        if((target_len == CH32MUX_HEADER_LEN) && (offset >= CH32MUX_HEADER_LEN))
        {
            ret = ch32mux_decode_header(frame, offset, &header);
            if(ret != CH32MUX_OK)
            {
                return ret;
            }
            target_len = header.total_len;
            if(target_len > frame_capacity)
            {
                return CH32MUX_ERR_OVERFLOW;
            }
        }
    }

    *frame_len = target_len;
    return CH32MUX_OK;
}

int ch32mux_read_hint(ch32mux_device_t *device, ch32mux_irq_hint_t *hint)
{
    uint8_t raw[CH32MUX_EP_HINT_SIZE];
    int transferred = 0;
    int ret;

    if((device == NULL) || (hint == NULL))
    {
        return CH32MUX_ERR_ARG;
    }

    ret = libusb_interrupt_transfer(device->handle,
                                    CH32MUX_EP_HINT_IN,
                                    raw,
                                    sizeof(raw),
                                    &transferred,
                                    device->timeout_ms);
    if(ret != LIBUSB_SUCCESS)
    {
        return map_libusb_error(ret);
    }
    if(transferred != (int)sizeof(raw))
    {
        return CH32MUX_ERR_PROTO;
    }

    return ch32mux_parse_irq_hint(raw, sizeof(raw), hint);
}

int ch32mux_debug_bulk_transfer(ch32mux_device_t *device,
                                uint8_t endpoint,
                                uint8_t *data,
                                int length,
                                int *transferred,
                                unsigned int timeout_ms,
                                int *libusb_status)
{
    int local_transferred = 0;
    int ret;

    if((device == NULL) || (data == NULL) || (length <= 0))
    {
        return CH32MUX_ERR_ARG;
    }

    ret = libusb_bulk_transfer(device->handle,
                               endpoint,
                               data,
                               length,
                               &local_transferred,
                               timeout_ms);
    if(transferred != NULL)
    {
        *transferred = local_transferred;
    }
    if(libusb_status != NULL)
    {
        *libusb_status = ret;
    }

    if(ret != LIBUSB_SUCCESS)
    {
        return map_libusb_error(ret);
    }
    return CH32MUX_OK;
}

uint16_t ch32mux_next_seq(ch32mux_device_t *device)
{
    uint16_t seq;

    if(device == NULL)
    {
        return 0;
    }

    seq = device->next_seq++;
    if(device->next_seq == 0)
    {
        device->next_seq = 1;
    }
    return seq;
}

unsigned int ch32mux_timeout_ms(const ch32mux_device_t *device)
{
    if(device == NULL)
    {
        return 0;
    }
    return device->timeout_ms;
}

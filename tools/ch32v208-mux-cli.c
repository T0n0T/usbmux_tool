#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

#include "ch32v208_mux/uart.h"

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s probe\n"
            "  %s heartbeat [text]\n"
            "  %s uart-cap <port>\n"
            "  %s uart-open <port> [baud]\n"
            "  %s uart-close <port>\n"
            "  %s debug-xfer\n",
            argv0,
            argv0,
            argv0,
            argv0,
            argv0,
            argv0);
}

static void dump_hex(const char *prefix, const uint8_t *data, size_t len)
{
    printf("%s (%zu bytes):", prefix, len);
    for(size_t i = 0; i < len; ++i)
    {
        if((i % 16U) == 0U)
        {
            printf("\n  %04zu:", i);
        }
        printf(" %02X", data[i]);
    }
    putchar('\n');
}

static int parse_port(const char *text, uint8_t *port)
{
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);

    if((end == text) || (*end != '\0') || (value > 255UL))
    {
        return -1;
    }

    *port = (uint8_t)value;
    return 0;
}

static int open_device(ch32mux_device_t **device)
{
    ch32mux_open_options_t options;
    int ret;

    ch32mux_default_open_options(&options);
    ret = ch32mux_open(&options, device);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr,
                "open %04X:%04X failed: %s\n",
                options.vid,
                options.pid,
                ch32mux_result_name(ret));
    }
    return ret;
}

static int cmd_probe(ch32mux_device_t *device)
{
    ch32mux_dev_info_t info;
    ch32mux_caps_t caps;
    int ret;

    ret = ch32mux_sys_get_dev_info(device, &info);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "GET_DEV_INFO failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    ret = ch32mux_sys_get_caps(device, &caps);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "GET_CAPS failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("proto=%u fw=%u.%u.%u uart_ports=%u ble_links=%u\n",
           info.proto_version,
           info.fw_major,
           info.fw_minor,
           info.fw_patch,
           info.uart_port_count,
           info.ble_max_links);
    printf("caps=0x%08X uart_ports=%u ble_links=%u net_reserved=%u\n",
           caps.caps_bitmap,
           caps.uart_port_count,
           caps.ble_max_links,
           caps.net_reserved);
    return 0;
}

static int cmd_heartbeat(ch32mux_device_t *device, const char *text)
{
    uint8_t response[128];
    size_t response_len = 0;
    int ret;

    ret = ch32mux_sys_heartbeat(device,
                                (const uint8_t *)text,
                                strlen(text),
                                response,
                                sizeof(response),
                                &response_len);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "HEARTBEAT failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("heartbeat response (%zu bytes): ", response_len);
    fwrite(response, 1, response_len, stdout);
    putchar('\n');
    return 0;
}

static int cmd_uart_cap(ch32mux_device_t *device, uint8_t port)
{
    ch32mux_uart_port_cap_t cap;
    int ret = ch32mux_uart_get_port_cap(device, port, &cap);

    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "UART_GET_PORT_CAP failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("port=%u phy_uart=%u state=%u rx_capacity=%u tx_capacity=%u line_coding=%u flush=%u\n",
           cap.logic_port,
           cap.phy_uart_id,
           cap.state,
           cap.rx_capacity,
           cap.tx_capacity,
           cap.supports_line_coding,
           cap.supports_flush);
    return 0;
}

static int cmd_uart_open(ch32mux_device_t *device, uint8_t port, const char *baud_text)
{
    ch32mux_uart_line_coding_t line = {
        .baudrate = 115200,
        .data_bits = 8,
        .parity = 0,
        .stop_bits = 1,
    };
    int ret;

    if(baud_text != NULL)
    {
        line.baudrate = (uint32_t)strtoul(baud_text, NULL, 0);
    }

    ret = ch32mux_uart_open(device, port, &line);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "UART_OPEN failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("uart %u opened at %u 8N1\n", port, line.baudrate);
    return 0;
}

static int cmd_debug_xfer(ch32mux_device_t *device)
{
    uint8_t out[CH32MUX_MAX_FRAME_LEN];
    uint8_t in[CH32MUX_MAX_FRAME_LEN];
    size_t out_len = 0;
    ch32mux_header_t req;
    ch32mux_header_t rsp;
    int transferred = 0;
    int libusb_status = 0;
    int ret;

    memset(&req, 0, sizeof(req));
    req.seq = ch32mux_next_seq(device);
    req.ch_type = CH32MUX_CH_SYS;
    req.ch_id = 0;
    req.msg_type = CH32MUX_MSG_CMD;
    req.opcode = CH32MUX_SYS_GET_DEV_INFO;

    ret = ch32mux_build_frame(out, sizeof(out), &out_len, &req, NULL, 0);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "build frame failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("timeout=%u ms\n", ch32mux_timeout_ms(device));
    dump_hex("EP2 OUT GET_DEV_INFO", out, out_len);

    ret = ch32mux_debug_bulk_transfer(device,
                                      CH32MUX_EP_FRAME_OUT,
                                      out,
                                      (int)out_len,
                                      &transferred,
                                      ch32mux_timeout_ms(device),
                                      &libusb_status);
    printf("bulk OUT ep=0x%02X ret=%s libusb=%d(%s) transferred=%d\n",
           CH32MUX_EP_FRAME_OUT,
           ch32mux_result_name(ret),
           libusb_status,
           libusb_error_name(libusb_status),
           transferred);
    if(ret != CH32MUX_OK)
    {
        return 1;
    }

    memset(in, 0, sizeof(in));
    transferred = 0;
    libusb_status = 0;
    ret = ch32mux_debug_bulk_transfer(device,
                                      CH32MUX_EP_FRAME_IN,
                                      in,
                                      CH32MUX_EP_PACKET_SIZE,
                                      &transferred,
                                      ch32mux_timeout_ms(device),
                                      &libusb_status);
    printf("bulk IN ep=0x%02X ret=%s libusb=%d(%s) transferred=%d\n",
           CH32MUX_EP_FRAME_IN,
           ch32mux_result_name(ret),
           libusb_status,
           libusb_error_name(libusb_status),
           transferred);
    if(transferred > 0)
    {
        dump_hex("EP2 IN raw", in, (size_t)transferred);
    }
    if(ret != CH32MUX_OK)
    {
        return 1;
    }

    ret = ch32mux_decode_header(in, (size_t)transferred, &rsp);
    printf("decode ret=%s\n", ch32mux_result_name(ret));
    if(ret != CH32MUX_OK)
    {
        return 1;
    }

    printf("rsp magic=0x%04X seq=%u ref=%u ch=0x%02X id=%u msg=0x%02X op=0x%02X status=0x%04X payload=%u total=%u\n",
           rsp.magic,
           rsp.seq,
           rsp.ref_seq,
           rsp.ch_type,
           rsp.ch_id,
           rsp.msg_type,
           rsp.opcode,
           rsp.status,
           rsp.payload_len,
           rsp.total_len);

    if((rsp.total_len > (uint16_t)transferred) &&
       (rsp.total_len <= CH32MUX_MAX_FRAME_LEN))
    {
        int remaining = (int)rsp.total_len - transferred;
        int transferred2 = 0;
        int libusb_status2 = 0;

        ret = ch32mux_debug_bulk_transfer(device,
                                          CH32MUX_EP_FRAME_IN,
                                          &in[transferred],
                                          remaining,
                                          &transferred2,
                                          ch32mux_timeout_ms(device),
                                          &libusb_status2);
        printf("bulk IN remainder ret=%s libusb=%d(%s) transferred=%d\n",
               ch32mux_result_name(ret),
               libusb_status2,
               libusb_error_name(libusb_status2),
               transferred2);
        transferred += transferred2;
        if(transferred2 > 0)
        {
            dump_hex("EP2 IN combined", in, (size_t)transferred);
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    ch32mux_device_t *device = NULL;
    int exit_code = 1;
    int ret;

    if(argc < 2)
    {
        usage(argv[0]);
        return 2;
    }

    ret = open_device(&device);
    if(ret != CH32MUX_OK)
    {
        return 1;
    }

    if(strcmp(argv[1], "probe") == 0)
    {
        exit_code = cmd_probe(device);
    }
    else if(strcmp(argv[1], "debug-xfer") == 0)
    {
        exit_code = cmd_debug_xfer(device);
    }
    else if(strcmp(argv[1], "heartbeat") == 0)
    {
        exit_code = cmd_heartbeat(device, argc >= 3 ? argv[2] : "ping");
    }
    else if(strcmp(argv[1], "uart-cap") == 0)
    {
        uint8_t port;
        if((argc != 3) || (parse_port(argv[2], &port) != 0))
        {
            usage(argv[0]);
            exit_code = 2;
        }
        else
        {
            exit_code = cmd_uart_cap(device, port);
        }
    }
    else if(strcmp(argv[1], "uart-open") == 0)
    {
        uint8_t port;
        if(((argc != 3) && (argc != 4)) || (parse_port(argv[2], &port) != 0))
        {
            usage(argv[0]);
            exit_code = 2;
        }
        else
        {
            exit_code = cmd_uart_open(device, port, argc == 4 ? argv[3] : NULL);
        }
    }
    else if(strcmp(argv[1], "uart-close") == 0)
    {
        uint8_t port;
        if((argc != 3) || (parse_port(argv[2], &port) != 0))
        {
            usage(argv[0]);
            exit_code = 2;
        }
        else
        {
            ret = ch32mux_uart_close(device, port);
            if(ret != CH32MUX_OK)
            {
                fprintf(stderr, "UART_CLOSE failed: %s\n", ch32mux_result_name(ret));
                exit_code = 1;
            }
            else
            {
                printf("uart %u closed\n", port);
                exit_code = 0;
            }
        }
    }
    else
    {
        usage(argv[0]);
        exit_code = 2;
    }

    ch32mux_close(device);
    return exit_code;
}

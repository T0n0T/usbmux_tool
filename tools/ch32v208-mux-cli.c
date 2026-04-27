#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libusb-1.0/libusb.h>

#include "ch32v208_mux/ble.h"
#include "ch32v208_mux/uart.h"

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s probe\n"
            "  %s heartbeat [text]\n"
            "  %s uart-cap <port>\n"
            "  %s uart-open <port> [baud]\n"
            "  %s uart-loopback <port> [baud] [payload]\n"
            "  %s uart-close <port>\n"
            "  %s ble-scan [seconds]\n"
            "  %s debug-xfer\n",
            argv0,
            argv0,
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

static int cmd_uart_loopback(ch32mux_device_t *device,
                             uint8_t port,
                             const char *baud_text,
                             const char *payload_text)
{
    ch32mux_uart_line_coding_t line = {
        .baudrate = 115200,
        .data_bits = 8,
        .parity = 0,
        .stop_bits = 1,
    };
    const uint8_t *payload = (const uint8_t *)payload_text;
    size_t payload_len = strlen(payload_text);
    uint8_t rx[CH32MUX_MAX_FRAME_LEN];
    size_t rx_total = 0;
    int ret;

    if(baud_text != NULL)
    {
        line.baudrate = (uint32_t)strtoul(baud_text, NULL, 0);
    }
    if(payload_len > (CH32MUX_MAX_FRAME_LEN - CH32MUX_HEADER_LEN))
    {
        fprintf(stderr, "payload too large: %zu\n", payload_len);
        return 2;
    }

    ret = ch32mux_uart_open(device, port, &line);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "UART_OPEN failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    ret = ch32mux_uart_write(device, port, payload, payload_len);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "UART_WRITE failed: %s\n", ch32mux_result_name(ret));
        (void)ch32mux_uart_close(device, port);
        return 1;
    }

    while(rx_total < payload_len)
    {
        uint8_t chunk[CH32MUX_MAX_FRAME_LEN];
        size_t chunk_len = 0;

        ret = ch32mux_uart_read(device, port, chunk, sizeof(chunk), &chunk_len);
        if(ret != CH32MUX_OK)
        {
            fprintf(stderr, "UART_READ failed after %zu/%zu bytes: %s\n",
                    rx_total,
                    payload_len,
                    ch32mux_result_name(ret));
            (void)ch32mux_uart_close(device, port);
            return 1;
        }
        if((chunk_len == 0U) || ((rx_total + chunk_len) > sizeof(rx)))
        {
            fprintf(stderr, "UART_READ invalid chunk len=%zu after %zu/%zu bytes\n",
                    chunk_len,
                    rx_total,
                    payload_len);
            (void)ch32mux_uart_close(device, port);
            return 1;
        }

        memcpy(&rx[rx_total], chunk, chunk_len);
        rx_total += chunk_len;
    }

    printf("uart %u loopback baud=%u tx=%zu rx=%zu\n", port, line.baudrate, payload_len, rx_total);
    dump_hex("TX", payload, payload_len);
    dump_hex("RX", rx, rx_total);

    ret = ch32mux_uart_close(device, port);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "UART_CLOSE failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    if((rx_total != payload_len) || (memcmp(rx, payload, payload_len) != 0))
    {
        fprintf(stderr, "loopback mismatch\n");
        return 1;
    }

    printf("loopback ok\n");
    return 0;
}

static void print_ble_addr(const uint8_t addr[6])
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X",
           addr[5],
           addr[4],
           addr[3],
           addr[2],
           addr[1],
           addr[0]);
}

static int cmd_ble_scan(ch32mux_device_t *device, const char *seconds_text)
{
    ch32mux_ble_cap_t cap;
    unsigned long seconds = 5UL;
    time_t end_time;
    unsigned int reports = 0;
    int ret;

    if(seconds_text != NULL)
    {
        seconds = strtoul(seconds_text, NULL, 0);
        if(seconds == 0UL)
        {
            seconds = 5UL;
        }
    }

    ret = ch32mux_ble_get_cap(device, &cap);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "BLE_GET_CAP failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("ble cap links=%u scan=%u connect=%u gatt=%u scan_int=%u scan_win=%u\n",
           cap.max_links,
           cap.supports_scan,
           cap.supports_connect,
           cap.supports_gatt,
           cap.scan_interval,
           cap.scan_window);

    ret = ch32mux_ble_set_scan_param(device,
                                     cap.scan_interval != 0U ? cap.scan_interval : 16U,
                                     cap.scan_window != 0U ? cap.scan_window : 16U,
                                     0U,
                                     0x03U,
                                     1U,
                                     0U);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "BLE_SET_SCAN_PARAM failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    ret = ch32mux_ble_scan_start(device);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "BLE_SCAN_START failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("ble scan started for %lu seconds\n", seconds);
    end_time = time(NULL) + (time_t)seconds;
    while(time(NULL) <= end_time)
    {
        ch32mux_header_t header;
        uint8_t payload[CH32MUX_MAX_FRAME_LEN];
        size_t payload_len = 0;

        ret = ch32mux_ble_read_event(device, &header, payload, sizeof(payload), &payload_len);
        if(ret == CH32MUX_ERR_TIMEOUT)
        {
            continue;
        }
        if(ret != CH32MUX_OK)
        {
            fprintf(stderr, "BLE event read failed: %s\n", ch32mux_result_name(ret));
            break;
        }

        if((header.ch_type == CH32MUX_CH_BLE_MGMT) &&
           (header.ch_id == 0U) &&
           (header.opcode == CH32MUX_BLE_EVT_SCAN_RSP))
        {
            ch32mux_ble_scan_result_t result;

            ret = ch32mux_ble_parse_scan_result(payload, payload_len, &result);
            if(ret != CH32MUX_OK)
            {
                fprintf(stderr, "BLE scan event parse failed: %s\n", ch32mux_result_name(ret));
                continue;
            }

            printf("scan[%u] addr=", reports);
            print_ble_addr(result.addr);
            printf(" type=%u event=%u rssi=%d adv_len=%u adv_prefix=",
                   result.addr_type,
                   result.event_type,
                   result.rssi,
                   result.adv_len);
            for(size_t i = 0; i < sizeof(result.adv_prefix); ++i)
            {
                printf("%02X", result.adv_prefix[i]);
            }
            putchar('\n');
            reports++;
        }
        else
        {
            printf("event ch=0x%02X id=%u op=0x%02X payload=%zu\n",
                   header.ch_type,
                   header.ch_id,
                   header.opcode,
                   payload_len);
        }
    }

    ret = ch32mux_ble_scan_stop(device);
    if(ret != CH32MUX_OK)
    {
        fprintf(stderr, "BLE_SCAN_STOP failed: %s\n", ch32mux_result_name(ret));
        return 1;
    }

    printf("ble scan stopped reports=%u\n", reports);
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
    dump_hex("EP3 OUT GET_DEV_INFO", out, out_len);

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
    else if(strcmp(argv[1], "uart-loopback") == 0)
    {
        uint8_t port;
        if(((argc < 3) || (argc > 5)) || (parse_port(argv[2], &port) != 0))
        {
            usage(argv[0]);
            exit_code = 2;
        }
        else
        {
            exit_code = cmd_uart_loopback(device,
                                          port,
                                          argc >= 4 ? argv[3] : NULL,
                                          argc >= 5 ? argv[4] : "ch32v208-loopback-0");
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
    else if(strcmp(argv[1], "ble-scan") == 0)
    {
        if(argc > 3)
        {
            usage(argv[0]);
            exit_code = 2;
        }
        else
        {
            exit_code = cmd_ble_scan(device, argc == 3 ? argv[2] : NULL);
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

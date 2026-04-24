#include "ch32v208_mux/uart.h"

#include <string.h>

static uint16_t read_le16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t read_le32(const uint8_t *src)
{
    return (uint32_t)src[0]
         | ((uint32_t)src[1] << 8)
         | ((uint32_t)src[2] << 16)
         | ((uint32_t)src[3] << 24);
}

static void write_le32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static int transact(ch32mux_device_t *device,
                    uint8_t ch_type,
                    uint8_t ch_id,
                    uint8_t msg_type,
                    uint8_t opcode,
                    const uint8_t *payload,
                    size_t payload_len,
                    ch32mux_header_t *rsp_header,
                    uint8_t *rsp_payload,
                    size_t rsp_capacity,
                    size_t *rsp_len)
{
    uint8_t out[CH32MUX_MAX_FRAME_LEN];
    uint8_t in[CH32MUX_MAX_FRAME_LEN];
    size_t out_len = 0;
    size_t in_len = 0;
    ch32mux_header_t req;
    ch32mux_header_t rsp;
    int ret;

    if(device == NULL)
    {
        return CH32MUX_ERR_ARG;
    }

    memset(&req, 0, sizeof(req));
    req.seq = ch32mux_next_seq(device);
    req.ch_type = ch_type;
    req.ch_id = ch_id;
    req.msg_type = msg_type;
    req.opcode = opcode;

    ret = ch32mux_build_frame(out, sizeof(out), &out_len, &req, payload, payload_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    ret = ch32mux_write_frame(device, out, out_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    ret = ch32mux_read_frame(device, in, sizeof(in), &in_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    ret = ch32mux_decode_header(in, in_len, &rsp);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if((rsp.msg_type != CH32MUX_MSG_RSP) ||
       (rsp.ref_seq != req.seq) ||
       (rsp.ch_type != ch_type) ||
       (rsp.ch_id != ch_id) ||
       (rsp.opcode != opcode))
    {
        return CH32MUX_ERR_PROTO;
    }
    if(rsp.status != CH32MUX_DEV_STATUS_OK)
    {
        return CH32MUX_ERR_DEVICE_STATUS;
    }

    if(rsp.payload_len > rsp_capacity)
    {
        return CH32MUX_ERR_OVERFLOW;
    }
    if((rsp.payload_len != 0U) && (rsp_payload == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if(rsp.payload_len != 0U)
    {
        memcpy(rsp_payload, &in[CH32MUX_HEADER_LEN], rsp.payload_len);
    }
    if(rsp_len != NULL)
    {
        *rsp_len = rsp.payload_len;
    }
    if(rsp_header != NULL)
    {
        *rsp_header = rsp;
    }

    return CH32MUX_OK;
}

int ch32mux_sys_get_dev_info(ch32mux_device_t *device, ch32mux_dev_info_t *info)
{
    uint8_t payload[sizeof(*info)];
    size_t payload_len = 0;
    int ret;

    if(info == NULL)
    {
        return CH32MUX_ERR_ARG;
    }

    ret = transact(device,
                   CH32MUX_CH_SYS,
                   0,
                   CH32MUX_MSG_CMD,
                   CH32MUX_SYS_GET_DEV_INFO,
                   NULL,
                   0,
                   NULL,
                   payload,
                   sizeof(payload),
                   &payload_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if(payload_len != sizeof(*info))
    {
        return CH32MUX_ERR_PROTO;
    }

    memcpy(info, payload, sizeof(*info));
    return CH32MUX_OK;
}

int ch32mux_sys_get_caps(ch32mux_device_t *device, ch32mux_caps_t *caps)
{
    uint8_t payload[8];
    size_t payload_len = 0;
    int ret;

    if(caps == NULL)
    {
        return CH32MUX_ERR_ARG;
    }

    ret = transact(device,
                   CH32MUX_CH_SYS,
                   0,
                   CH32MUX_MSG_CMD,
                   CH32MUX_SYS_GET_CAPS,
                   NULL,
                   0,
                   NULL,
                   payload,
                   sizeof(payload),
                   &payload_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if(payload_len != sizeof(payload))
    {
        return CH32MUX_ERR_PROTO;
    }

    caps->caps_bitmap = read_le32(&payload[0]);
    caps->uart_port_count = payload[4];
    caps->ble_max_links = payload[5];
    caps->net_reserved = payload[6];
    caps->reserved0 = payload[7];
    return CH32MUX_OK;
}

int ch32mux_sys_heartbeat(ch32mux_device_t *device,
                          const uint8_t *payload,
                          size_t payload_len,
                          uint8_t *response,
                          size_t response_capacity,
                          size_t *response_len)
{
    return transact(device,
                    CH32MUX_CH_SYS,
                    0,
                    CH32MUX_MSG_CMD,
                    CH32MUX_SYS_HEARTBEAT,
                    payload,
                    payload_len,
                    NULL,
                    response,
                    response_capacity,
                    response_len);
}

int ch32mux_uart_get_port_cap(ch32mux_device_t *device, uint8_t port, ch32mux_uart_port_cap_t *cap)
{
    uint8_t payload[12];
    size_t payload_len = 0;
    int ret;

    if(cap == NULL)
    {
        return CH32MUX_ERR_ARG;
    }

    ret = transact(device,
                   CH32MUX_CH_UART_CTRL,
                   port,
                   CH32MUX_MSG_CMD,
                   CH32MUX_UART_GET_PORT_CAP,
                   NULL,
                   0,
                   NULL,
                   payload,
                   sizeof(payload),
                   &payload_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if(payload_len != sizeof(payload))
    {
        return CH32MUX_ERR_PROTO;
    }

    cap->logic_port = payload[0];
    cap->phy_uart_id = payload[1];
    cap->state = payload[2];
    cap->reserved0 = payload[3];
    cap->rx_capacity = read_le16(&payload[4]);
    cap->tx_capacity = read_le16(&payload[6]);
    cap->supports_line_coding = payload[8];
    cap->supports_flush = payload[9];
    cap->reserved1 = payload[10];
    cap->reserved2 = payload[11];
    return CH32MUX_OK;
}

int ch32mux_uart_open(ch32mux_device_t *device, uint8_t port, const ch32mux_uart_line_coding_t *line)
{
    ch32mux_uart_line_coding_t local_line;
    uint8_t payload[8];

    if(line == NULL)
    {
        local_line.baudrate = 115200;
        local_line.data_bits = 8;
        local_line.parity = 0;
        local_line.stop_bits = 1;
        line = &local_line;
    }

    write_le32(&payload[0], line->baudrate);
    payload[4] = line->data_bits;
    payload[5] = line->parity;
    payload[6] = line->stop_bits;
    payload[7] = 0;

    return transact(device,
                    CH32MUX_CH_UART_CTRL,
                    port,
                    CH32MUX_MSG_CMD,
                    CH32MUX_UART_OPEN,
                    payload,
                    sizeof(payload),
                    NULL,
                    NULL,
                    0,
                    NULL);
}

int ch32mux_uart_close(ch32mux_device_t *device, uint8_t port)
{
    return transact(device,
                    CH32MUX_CH_UART_CTRL,
                    port,
                    CH32MUX_MSG_CMD,
                    CH32MUX_UART_CLOSE,
                    NULL,
                    0,
                    NULL,
                    NULL,
                    0,
                    NULL);
}

int ch32mux_uart_get_stats(ch32mux_device_t *device, uint8_t port, ch32mux_uart_stats_t *stats)
{
    uint8_t payload[20];
    size_t payload_len = 0;
    int ret;

    if(stats == NULL)
    {
        return CH32MUX_ERR_ARG;
    }

    ret = transact(device,
                   CH32MUX_CH_UART_CTRL,
                   port,
                   CH32MUX_MSG_CMD,
                   CH32MUX_UART_GET_STATS,
                   NULL,
                   0,
                   NULL,
                   payload,
                   sizeof(payload),
                   &payload_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if(payload_len != sizeof(payload))
    {
        return CH32MUX_ERR_PROTO;
    }

    stats->rx_bytes = read_le32(&payload[0]);
    stats->tx_bytes = read_le32(&payload[4]);
    stats->drop_rx_bytes = read_le32(&payload[8]);
    stats->drop_tx_bytes = read_le32(&payload[12]);
    stats->state = payload[16];
    stats->data_bits = payload[17];
    stats->parity = payload[18];
    stats->stop_bits = payload[19];
    return CH32MUX_OK;
}

int ch32mux_uart_write(ch32mux_device_t *device, uint8_t port, const uint8_t *data, size_t len)
{
    uint8_t out[CH32MUX_MAX_FRAME_LEN];
    size_t out_len = 0;
    ch32mux_header_t header;
    int ret;

    if((device == NULL) || ((data == NULL) && (len != 0U)))
    {
        return CH32MUX_ERR_ARG;
    }

    memset(&header, 0, sizeof(header));
    header.seq = ch32mux_next_seq(device);
    header.ch_type = CH32MUX_CH_UART_DATA;
    header.ch_id = port;
    header.msg_type = CH32MUX_MSG_DATA;
    header.opcode = 0;

    ret = ch32mux_build_frame(out, sizeof(out), &out_len, &header, data, len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    return ch32mux_write_frame(device, out, out_len);
}

int ch32mux_uart_read(ch32mux_device_t *device,
                      uint8_t port,
                      uint8_t *data,
                      size_t capacity,
                      size_t *len)
{
    uint8_t frame[CH32MUX_MAX_FRAME_LEN];
    size_t frame_len = 0;
    ch32mux_header_t header;
    int ret;

    if((device == NULL) || (data == NULL) || (len == NULL))
    {
        return CH32MUX_ERR_ARG;
    }

    ret = ch32mux_read_frame(device, frame, sizeof(frame), &frame_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    ret = ch32mux_decode_header(frame, frame_len, &header);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if((header.ch_type != CH32MUX_CH_UART_DATA) ||
       (header.ch_id != port) ||
       (header.msg_type != CH32MUX_MSG_DATA))
    {
        return CH32MUX_ERR_PROTO;
    }
    if(header.payload_len > capacity)
    {
        return CH32MUX_ERR_OVERFLOW;
    }

    memcpy(data, &frame[CH32MUX_HEADER_LEN], header.payload_len);
    *len = header.payload_len;
    return CH32MUX_OK;
}

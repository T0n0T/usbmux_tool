#include "ch32v208_mux/ble.h"

#include <string.h>

static uint16_t read_le16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static void write_le16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static int transact(ch32mux_device_t *device,
                    uint8_t opcode,
                    const uint8_t *payload,
                    size_t payload_len,
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
    req.ch_type = CH32MUX_CH_BLE_MGMT;
    req.ch_id = 0;
    req.msg_type = CH32MUX_MSG_CMD;
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
       (rsp.ch_type != CH32MUX_CH_BLE_MGMT) ||
       (rsp.ch_id != 0U) ||
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

    return CH32MUX_OK;
}

int ch32mux_ble_get_cap(ch32mux_device_t *device, ch32mux_ble_cap_t *cap)
{
    uint8_t payload[8];
    size_t payload_len = 0;
    int ret;

    if(cap == NULL)
    {
        return CH32MUX_ERR_ARG;
    }

    ret = transact(device, CH32MUX_BLE_GET_CAP, NULL, 0, payload, sizeof(payload), &payload_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if(payload_len != sizeof(payload))
    {
        return CH32MUX_ERR_PROTO;
    }

    cap->max_links = payload[0];
    cap->supports_scan = payload[1];
    cap->supports_connect = payload[2];
    cap->supports_gatt = payload[3];
    cap->scan_interval = read_le16(&payload[4]);
    cap->scan_window = read_le16(&payload[6]);
    return CH32MUX_OK;
}

int ch32mux_ble_set_scan_param(ch32mux_device_t *device,
                               uint16_t interval,
                               uint16_t window,
                               uint16_t duration,
                               uint8_t mode,
                               uint8_t active,
                               uint8_t white_list)
{
    uint8_t payload[9];

    write_le16(&payload[0], interval);
    write_le16(&payload[2], window);
    write_le16(&payload[4], duration);
    payload[6] = mode;
    payload[7] = active;
    payload[8] = white_list;

    return transact(device,
                    CH32MUX_BLE_SET_SCAN_PARAM,
                    payload,
                    sizeof(payload),
                    NULL,
                    0,
                    NULL);
}

int ch32mux_ble_scan_start(ch32mux_device_t *device)
{
    return transact(device, CH32MUX_BLE_SCAN_START, NULL, 0, NULL, 0, NULL);
}

int ch32mux_ble_scan_stop(ch32mux_device_t *device)
{
    return transact(device, CH32MUX_BLE_SCAN_STOP, NULL, 0, NULL, 0, NULL);
}

int ch32mux_ble_parse_scan_result(const uint8_t *payload,
                                  size_t payload_len,
                                  ch32mux_ble_scan_result_t *result)
{
    if((payload == NULL) || (result == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if(payload_len < 16U)
    {
        return CH32MUX_ERR_PROTO;
    }

    result->addr_type = payload[0];
    result->event_type = payload[1];
    result->rssi = (int8_t)payload[2];
    result->adv_len = payload[3];
    memcpy(result->addr, &payload[4], sizeof(result->addr));
    memcpy(result->adv_prefix, &payload[10], sizeof(result->adv_prefix));
    return CH32MUX_OK;
}

int ch32mux_ble_read_event(ch32mux_device_t *device,
                           ch32mux_header_t *header,
                           uint8_t *payload,
                           size_t payload_capacity,
                           size_t *payload_len)
{
    uint8_t frame[CH32MUX_MAX_FRAME_LEN];
    size_t frame_len = 0;
    ch32mux_header_t local_header;
    int ret;

    if((device == NULL) || (payload_len == NULL))
    {
        return CH32MUX_ERR_ARG;
    }

    ret = ch32mux_read_frame(device, frame, sizeof(frame), &frame_len);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    ret = ch32mux_decode_header(frame, frame_len, &local_header);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }
    if(local_header.msg_type != CH32MUX_MSG_EVT)
    {
        return CH32MUX_ERR_PROTO;
    }
    if(local_header.payload_len > payload_capacity)
    {
        return CH32MUX_ERR_OVERFLOW;
    }
    if((local_header.payload_len != 0U) && (payload == NULL))
    {
        return CH32MUX_ERR_ARG;
    }

    if(local_header.payload_len != 0U)
    {
        memcpy(payload, &frame[CH32MUX_HEADER_LEN], local_header.payload_len);
    }
    *payload_len = local_header.payload_len;
    if(header != NULL)
    {
        *header = local_header;
    }

    return CH32MUX_OK;
}

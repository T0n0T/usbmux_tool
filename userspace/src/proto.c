#include "ch32v208_mux/proto.h"

#include <string.h>

enum {
    HDR_MAGIC_OFFSET = 0,
    HDR_VERSION_OFFSET = 2,
    HDR_HEADER_LEN_OFFSET = 3,
    HDR_TOTAL_LEN_OFFSET = 4,
    HDR_SEQ_OFFSET = 6,
    HDR_REF_SEQ_OFFSET = 8,
    HDR_CH_TYPE_OFFSET = 10,
    HDR_CH_ID_OFFSET = 11,
    HDR_MSG_TYPE_OFFSET = 12,
    HDR_OPCODE_OFFSET = 13,
    HDR_FLAGS_OFFSET = 14,
    HDR_STATUS_OFFSET = 16,
    HDR_PAYLOAD_LEN_OFFSET = 18,
    HDR_CRC16_OFFSET = 20,
    HDR_RESERVED_OFFSET = 22,
};

static void write_le16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static uint16_t read_le16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static void serialize_header(uint8_t *dst, const ch32mux_header_t *header, uint16_t crc)
{
    write_le16(&dst[HDR_MAGIC_OFFSET], header->magic);
    dst[HDR_VERSION_OFFSET] = header->version;
    dst[HDR_HEADER_LEN_OFFSET] = header->header_len;
    write_le16(&dst[HDR_TOTAL_LEN_OFFSET], header->total_len);
    write_le16(&dst[HDR_SEQ_OFFSET], header->seq);
    write_le16(&dst[HDR_REF_SEQ_OFFSET], header->ref_seq);
    dst[HDR_CH_TYPE_OFFSET] = header->ch_type;
    dst[HDR_CH_ID_OFFSET] = header->ch_id;
    dst[HDR_MSG_TYPE_OFFSET] = header->msg_type;
    dst[HDR_OPCODE_OFFSET] = header->opcode;
    write_le16(&dst[HDR_FLAGS_OFFSET], header->flags);
    write_le16(&dst[HDR_STATUS_OFFSET], header->status);
    write_le16(&dst[HDR_PAYLOAD_LEN_OFFSET], header->payload_len);
    write_le16(&dst[HDR_CRC16_OFFSET], crc);
    write_le16(&dst[HDR_RESERVED_OFFSET], header->reserved);
}

static void deserialize_header(ch32mux_header_t *header, const uint8_t *src)
{
    header->magic = read_le16(&src[HDR_MAGIC_OFFSET]);
    header->version = src[HDR_VERSION_OFFSET];
    header->header_len = src[HDR_HEADER_LEN_OFFSET];
    header->total_len = read_le16(&src[HDR_TOTAL_LEN_OFFSET]);
    header->seq = read_le16(&src[HDR_SEQ_OFFSET]);
    header->ref_seq = read_le16(&src[HDR_REF_SEQ_OFFSET]);
    header->ch_type = src[HDR_CH_TYPE_OFFSET];
    header->ch_id = src[HDR_CH_ID_OFFSET];
    header->msg_type = src[HDR_MSG_TYPE_OFFSET];
    header->opcode = src[HDR_OPCODE_OFFSET];
    header->flags = read_le16(&src[HDR_FLAGS_OFFSET]);
    header->status = read_le16(&src[HDR_STATUS_OFFSET]);
    header->payload_len = read_le16(&src[HDR_PAYLOAD_LEN_OFFSET]);
    header->header_crc16 = read_le16(&src[HDR_CRC16_OFFSET]);
    header->reserved = read_le16(&src[HDR_RESERVED_OFFSET]);
}

static int check_header_shape(const ch32mux_header_t *header)
{
    if(header == NULL)
    {
        return CH32MUX_ERR_ARG;
    }
    if(header->magic != CH32MUX_MAGIC)
    {
        return CH32MUX_ERR_PROTO;
    }
    if(header->version != CH32MUX_VERSION)
    {
        return CH32MUX_ERR_PROTO;
    }
    if(header->header_len != CH32MUX_HEADER_LEN)
    {
        return CH32MUX_ERR_PROTO;
    }
    if(header->total_len != (uint16_t)(header->header_len + header->payload_len))
    {
        return CH32MUX_ERR_PROTO;
    }
    if(header->total_len > CH32MUX_MAX_FRAME_LEN)
    {
        return CH32MUX_ERR_OVERFLOW;
    }
    return CH32MUX_OK;
}

uint16_t ch32mux_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;

    if(data == NULL)
    {
        return 0;
    }

    for(size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for(uint8_t bit = 0; bit < 8; ++bit)
        {
            if((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

int ch32mux_encode_header(uint8_t *dst, size_t dst_len, ch32mux_header_t *header)
{
    uint8_t header_bytes[CH32MUX_HEADER_LEN];

    if((dst == NULL) || (header == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if(dst_len < CH32MUX_HEADER_LEN)
    {
        return CH32MUX_ERR_OVERFLOW;
    }
    if(header->payload_len > (CH32MUX_MAX_FRAME_LEN - CH32MUX_HEADER_LEN))
    {
        return CH32MUX_ERR_OVERFLOW;
    }

    header->magic = CH32MUX_MAGIC;
    header->version = CH32MUX_VERSION;
    header->header_len = CH32MUX_HEADER_LEN;
    header->total_len = (uint16_t)(CH32MUX_HEADER_LEN + header->payload_len);
    header->header_crc16 = 0;

    serialize_header(header_bytes, header, 0);
    header->header_crc16 = ch32mux_crc16(header_bytes, sizeof(header_bytes));
    serialize_header(dst, header, header->header_crc16);

    return CH32MUX_OK;
}

int ch32mux_decode_header(const uint8_t *src, size_t src_len, ch32mux_header_t *header)
{
    uint8_t header_bytes[CH32MUX_HEADER_LEN];
    ch32mux_header_t local;
    int ret;

    if((src == NULL) || (header == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if(src_len < CH32MUX_HEADER_LEN)
    {
        return CH32MUX_ERR_PROTO;
    }

    deserialize_header(&local, src);
    ret = check_header_shape(&local);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    memcpy(header_bytes, src, CH32MUX_HEADER_LEN);
    header_bytes[HDR_CRC16_OFFSET] = 0;
    header_bytes[HDR_CRC16_OFFSET + 1] = 0;

    if(ch32mux_crc16(header_bytes, sizeof(header_bytes)) != local.header_crc16)
    {
        return CH32MUX_ERR_PROTO;
    }

    *header = local;
    return CH32MUX_OK;
}

int ch32mux_build_frame(uint8_t *dst,
                        size_t dst_len,
                        size_t *frame_len,
                        ch32mux_header_t *header,
                        const uint8_t *payload,
                        size_t payload_len)
{
    int ret;

    if((dst == NULL) || (frame_len == NULL) || (header == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if((payload == NULL) && (payload_len != 0U))
    {
        return CH32MUX_ERR_ARG;
    }
    if(payload_len > (CH32MUX_MAX_FRAME_LEN - CH32MUX_HEADER_LEN))
    {
        return CH32MUX_ERR_OVERFLOW;
    }
    if(dst_len < (CH32MUX_HEADER_LEN + payload_len))
    {
        return CH32MUX_ERR_OVERFLOW;
    }

    header->payload_len = (uint16_t)payload_len;
    ret = ch32mux_encode_header(dst, dst_len, header);
    if(ret != CH32MUX_OK)
    {
        return ret;
    }

    if(payload_len != 0U)
    {
        memcpy(&dst[CH32MUX_HEADER_LEN], payload, payload_len);
    }
    *frame_len = CH32MUX_HEADER_LEN + payload_len;

    return CH32MUX_OK;
}

int ch32mux_parse_irq_hint(const uint8_t *src, size_t src_len, ch32mux_irq_hint_t *hint)
{
    if((src == NULL) || (hint == NULL))
    {
        return CH32MUX_ERR_ARG;
    }
    if(src_len < CH32MUX_EP_HINT_SIZE)
    {
        return CH32MUX_ERR_PROTO;
    }
    if(src[0] != CH32MUX_IRQ_HINT_VERSION)
    {
        return CH32MUX_ERR_PROTO;
    }

    hint->version = src[0];
    hint->urgent_flags = src[1];
    hint->pending_bitmap = read_le16(&src[2]);
    hint->dropped_bitmap = read_le16(&src[4]);
    hint->reserved = read_le16(&src[6]);
    return CH32MUX_OK;
}

const char *ch32mux_result_name(int result)
{
    switch(result)
    {
        case CH32MUX_OK:
            return "OK";
        case CH32MUX_ERR_ARG:
            return "ERR_ARG";
        case CH32MUX_ERR_NO_MEM:
            return "ERR_NO_MEM";
        case CH32MUX_ERR_PROTO:
            return "ERR_PROTO";
        case CH32MUX_ERR_TIMEOUT:
            return "ERR_TIMEOUT";
        case CH32MUX_ERR_IO:
            return "ERR_IO";
        case CH32MUX_ERR_NOT_FOUND:
            return "ERR_NOT_FOUND";
        case CH32MUX_ERR_DEVICE_STATUS:
            return "ERR_DEVICE_STATUS";
        case CH32MUX_ERR_OVERFLOW:
            return "ERR_OVERFLOW";
        default:
            return "ERR_UNKNOWN";
    }
}

const char *ch32mux_device_status_name(uint16_t status)
{
    switch(status)
    {
        case CH32MUX_DEV_STATUS_OK:
            return "OK";
        case CH32MUX_DEV_STATUS_ERR_BAD_MAGIC:
            return "ERR_BAD_MAGIC";
        case CH32MUX_DEV_STATUS_ERR_BAD_VERSION:
            return "ERR_BAD_VERSION";
        case CH32MUX_DEV_STATUS_ERR_BAD_LEN:
            return "ERR_BAD_LEN";
        case CH32MUX_DEV_STATUS_ERR_BAD_HDR_CRC:
            return "ERR_BAD_HDR_CRC";
        case CH32MUX_DEV_STATUS_ERR_BAD_PAYLOAD_CRC:
            return "ERR_BAD_PAYLOAD_CRC";
        case CH32MUX_DEV_STATUS_ERR_UNSUPPORTED_CH:
            return "ERR_UNSUPPORTED_CH";
        case CH32MUX_DEV_STATUS_ERR_UNSUPPORTED_OPCODE:
            return "ERR_UNSUPPORTED_OPCODE";
        case CH32MUX_DEV_STATUS_ERR_INVALID_PARAM:
            return "ERR_INVALID_PARAM";
        case CH32MUX_DEV_STATUS_ERR_INVALID_STATE:
            return "ERR_INVALID_STATE";
        case CH32MUX_DEV_STATUS_ERR_BUSY:
            return "ERR_BUSY";
        case CH32MUX_DEV_STATUS_ERR_NO_RESOURCE:
            return "ERR_NO_RESOURCE";
        case CH32MUX_DEV_STATUS_ERR_TIMEOUT:
            return "ERR_TIMEOUT";
        case CH32MUX_DEV_STATUS_ERR_OVERFLOW:
            return "ERR_OVERFLOW";
        case CH32MUX_DEV_STATUS_ERR_UART_MAP_INVALID:
            return "ERR_UART_MAP_INVALID";
        case CH32MUX_DEV_STATUS_ERR_UART_NOT_OPEN:
            return "ERR_UART_NOT_OPEN";
        case CH32MUX_DEV_STATUS_ERR_BLE_SLOT_INVALID:
            return "ERR_BLE_SLOT_INVALID";
        case CH32MUX_DEV_STATUS_ERR_BLE_NOT_CONNECTED:
            return "ERR_BLE_NOT_CONNECTED";
        case CH32MUX_DEV_STATUS_ERR_BLE_DISC_NOT_READY:
            return "ERR_BLE_DISC_NOT_READY";
        case CH32MUX_DEV_STATUS_ERR_BLE_ATT:
            return "ERR_BLE_ATT";
        case CH32MUX_DEV_STATUS_ERR_INTERNAL:
            return "ERR_INTERNAL";
        default:
            return "ERR_UNKNOWN_DEVICE_STATUS";
    }
}

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32v208_mux/proto.h"

static int failures;

#define EXPECT_TRUE(expr) \
    do { \
        if(!(expr)) { \
            fprintf(stderr, "%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
            failures++; \
        } \
    } while(0)

#define EXPECT_EQ_U16(actual, expected) \
    do { \
        uint16_t a_ = (uint16_t)(actual); \
        uint16_t e_ = (uint16_t)(expected); \
        if(a_ != e_) { \
            fprintf(stderr, "%s:%d: expected 0x%04X, got 0x%04X\n", __FILE__, __LINE__, e_, a_); \
            failures++; \
        } \
    } while(0)

static void test_encode_decode_header(void)
{
    uint8_t frame[CH32MUX_MAX_FRAME_LEN];
    uint8_t payload[] = {0x11, 0x22, 0x33};
    size_t frame_len = 0;
    ch32mux_header_t out;
    ch32mux_header_t in;

    memset(&out, 0, sizeof(out));
    out.seq = 7;
    out.ref_seq = 3;
    out.ch_type = CH32MUX_CH_SYS;
    out.msg_type = CH32MUX_MSG_CMD;
    out.opcode = CH32MUX_SYS_HEARTBEAT;

    EXPECT_TRUE(ch32mux_build_frame(frame, sizeof(frame), &frame_len, &out, payload, sizeof(payload)) == CH32MUX_OK);
    EXPECT_EQ_U16(frame_len, CH32MUX_HEADER_LEN + sizeof(payload));
    EXPECT_TRUE(ch32mux_decode_header(frame, frame_len, &in) == CH32MUX_OK);
    EXPECT_EQ_U16(in.magic, CH32MUX_MAGIC);
    EXPECT_EQ_U16(in.total_len, CH32MUX_HEADER_LEN + sizeof(payload));
    EXPECT_EQ_U16(in.payload_len, sizeof(payload));
    EXPECT_EQ_U16(in.seq, 7);
    EXPECT_TRUE(memcmp(&frame[CH32MUX_HEADER_LEN], payload, sizeof(payload)) == 0);
}

static void test_decode_rejects_bad_crc(void)
{
    uint8_t frame[CH32MUX_MAX_FRAME_LEN];
    size_t frame_len = 0;
    ch32mux_header_t out;
    ch32mux_header_t in;

    memset(&out, 0, sizeof(out));
    out.seq = 1;
    out.ch_type = CH32MUX_CH_SYS;
    out.msg_type = CH32MUX_MSG_CMD;
    out.opcode = CH32MUX_SYS_GET_CAPS;

    EXPECT_TRUE(ch32mux_build_frame(frame, sizeof(frame), &frame_len, &out, NULL, 0) == CH32MUX_OK);
    frame[0] ^= 0x01U;
    EXPECT_TRUE(ch32mux_decode_header(frame, frame_len, &in) == CH32MUX_ERR_PROTO);
}

static void test_build_rejects_oversize_payload(void)
{
    uint8_t frame[CH32MUX_MAX_FRAME_LEN];
    uint8_t payload[CH32MUX_MAX_FRAME_LEN];
    size_t frame_len = 0;
    ch32mux_header_t out;

    memset(&out, 0, sizeof(out));
    out.ch_type = CH32MUX_CH_UART_DATA;
    out.ch_id = 1;
    out.msg_type = CH32MUX_MSG_DATA;

    EXPECT_TRUE(ch32mux_build_frame(frame,
                                    sizeof(frame),
                                    &frame_len,
                                    &out,
                                    payload,
                                    sizeof(payload)) == CH32MUX_ERR_OVERFLOW);
}

static void test_parse_irq_hint(void)
{
    uint8_t raw[CH32MUX_EP_HINT_SIZE] = {
        CH32MUX_IRQ_HINT_VERSION, 0x05, 0x07, 0x00, 0x02, 0x00, 0x00, 0x00
    };
    ch32mux_irq_hint_t hint;

    EXPECT_TRUE(ch32mux_parse_irq_hint(raw, sizeof(raw), &hint) == CH32MUX_OK);
    EXPECT_EQ_U16(hint.pending_bitmap, CH32MUX_PENDING_RSP | CH32MUX_PENDING_EVT | CH32MUX_PENDING_DATA);
    EXPECT_EQ_U16(hint.dropped_bitmap, CH32MUX_PENDING_EVT);
}

int main(void)
{
    test_encode_decode_header();
    test_decode_rejects_bad_crc();
    test_build_rejects_oversize_payload();
    test_parse_irq_hint();

    return failures == 0 ? 0 : 1;
}

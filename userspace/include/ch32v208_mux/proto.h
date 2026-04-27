#ifndef CH32V208_MUX_PROTO_H
#define CH32V208_MUX_PROTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32MUX_USB_VID              0x1A86U
#define CH32MUX_USB_PID              0x2080U
#define CH32MUX_USB_INTERFACE        0U
#define CH32MUX_EP_HINT_IN           0x81U
#define CH32MUX_EP_FRAME_OUT         0x03U
#define CH32MUX_EP_FRAME_IN          0x82U
#define CH32MUX_EP_HINT_SIZE         8U
#define CH32MUX_EP_PACKET_SIZE       64U
#define CH32MUX_MAX_FRAME_LEN        512U

#define CH32MUX_MAGIC                0xA55AU
#define CH32MUX_VERSION              0x01U
#define CH32MUX_HEADER_LEN           24U
#define CH32MUX_IRQ_HINT_VERSION     0x01U

typedef enum {
    CH32MUX_OK = 0,
    CH32MUX_ERR_ARG = -1,
    CH32MUX_ERR_NO_MEM = -2,
    CH32MUX_ERR_PROTO = -3,
    CH32MUX_ERR_TIMEOUT = -4,
    CH32MUX_ERR_IO = -5,
    CH32MUX_ERR_NOT_FOUND = -6,
    CH32MUX_ERR_DEVICE_STATUS = -7,
    CH32MUX_ERR_OVERFLOW = -8,
} ch32mux_result_t;

typedef enum {
    CH32MUX_CH_SYS       = 0x00,
    CH32MUX_CH_UART_CTRL = 0x01,
    CH32MUX_CH_UART_DATA = 0x02,
    CH32MUX_CH_BLE_MGMT  = 0x10,
    CH32MUX_CH_BLE_CONN  = 0x11,
    CH32MUX_CH_NET_MGMT  = 0x20,
    CH32MUX_CH_NET_DATA  = 0x21,
} ch32mux_channel_type_t;

typedef enum {
    CH32MUX_MSG_CMD  = 0x01,
    CH32MUX_MSG_RSP  = 0x02,
    CH32MUX_MSG_EVT  = 0x03,
    CH32MUX_MSG_DATA = 0x04,
} ch32mux_msg_type_t;

typedef enum {
    CH32MUX_SYS_GET_DEV_INFO  = 0x01,
    CH32MUX_SYS_GET_CAPS      = 0x02,
    CH32MUX_SYS_GET_STATS     = 0x03,
    CH32MUX_SYS_CLEAR_STATS   = 0x04,
    CH32MUX_SYS_SET_LOG_LEVEL = 0x05,
    CH32MUX_SYS_GET_LOG_LEVEL = 0x06,
    CH32MUX_SYS_HEARTBEAT     = 0x07,
    CH32MUX_SYS_SOFT_RESET    = 0x08,
} ch32mux_sys_opcode_t;

typedef enum {
    CH32MUX_UART_GET_PORT_CAP      = 0x01,
    CH32MUX_UART_GET_PORT_MAP      = 0x02,
    CH32MUX_UART_OPEN              = 0x03,
    CH32MUX_UART_CLOSE             = 0x04,
    CH32MUX_UART_SET_LINE_CODING   = 0x05,
    CH32MUX_UART_SET_FLOW_CTRL     = 0x06,
    CH32MUX_UART_SET_MODEM_SIGNALS = 0x07,
    CH32MUX_UART_GET_MODEM_STATUS  = 0x08,
    CH32MUX_UART_SEND_BREAK        = 0x09,
    CH32MUX_UART_FLUSH_RX          = 0x0A,
    CH32MUX_UART_FLUSH_TX          = 0x0B,
    CH32MUX_UART_GET_STATS         = 0x0C,
} ch32mux_uart_opcode_t;

typedef enum {
    CH32MUX_BLE_GET_CAP        = 0x01,
    CH32MUX_BLE_SET_SCAN_PARAM = 0x02,
    CH32MUX_BLE_SCAN_START     = 0x03,
    CH32MUX_BLE_SCAN_STOP      = 0x04,
    CH32MUX_BLE_CONNECT        = 0x05,
    CH32MUX_BLE_DISCONNECT     = 0x06,
    CH32MUX_BLE_GET_CONN_STATE = 0x07,
    CH32MUX_BLE_EVT_SCAN_RSP   = 0x80,
    CH32MUX_BLE_EVT_CONN_STATE = 0x81,
} ch32mux_ble_opcode_t;

typedef enum {
    CH32MUX_DEV_STATUS_OK                     = 0x0000,
    CH32MUX_DEV_STATUS_ERR_BAD_MAGIC          = 0x0001,
    CH32MUX_DEV_STATUS_ERR_BAD_VERSION        = 0x0002,
    CH32MUX_DEV_STATUS_ERR_BAD_LEN            = 0x0003,
    CH32MUX_DEV_STATUS_ERR_BAD_HDR_CRC        = 0x0004,
    CH32MUX_DEV_STATUS_ERR_BAD_PAYLOAD_CRC    = 0x0005,
    CH32MUX_DEV_STATUS_ERR_UNSUPPORTED_CH     = 0x0006,
    CH32MUX_DEV_STATUS_ERR_UNSUPPORTED_OPCODE = 0x0007,
    CH32MUX_DEV_STATUS_ERR_INVALID_PARAM      = 0x0008,
    CH32MUX_DEV_STATUS_ERR_INVALID_STATE      = 0x0009,
    CH32MUX_DEV_STATUS_ERR_BUSY               = 0x000A,
    CH32MUX_DEV_STATUS_ERR_NO_RESOURCE        = 0x000B,
    CH32MUX_DEV_STATUS_ERR_TIMEOUT            = 0x000C,
    CH32MUX_DEV_STATUS_ERR_OVERFLOW           = 0x000D,
    CH32MUX_DEV_STATUS_ERR_UART_MAP_INVALID   = 0x0010,
    CH32MUX_DEV_STATUS_ERR_UART_NOT_OPEN      = 0x0011,
    CH32MUX_DEV_STATUS_ERR_BLE_SLOT_INVALID   = 0x0020,
    CH32MUX_DEV_STATUS_ERR_BLE_NOT_CONNECTED  = 0x0021,
    CH32MUX_DEV_STATUS_ERR_BLE_DISC_NOT_READY = 0x0022,
    CH32MUX_DEV_STATUS_ERR_BLE_ATT            = 0x0023,
    CH32MUX_DEV_STATUS_ERR_INTERNAL           = 0x0030,
} ch32mux_device_status_t;

typedef enum {
    CH32MUX_PENDING_RSP  = 0x0001,
    CH32MUX_PENDING_EVT  = 0x0002,
    CH32MUX_PENDING_DATA = 0x0004,
} ch32mux_pending_bits_t;

typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t header_len;
    uint16_t total_len;
    uint16_t seq;
    uint16_t ref_seq;
    uint8_t ch_type;
    uint8_t ch_id;
    uint8_t msg_type;
    uint8_t opcode;
    uint16_t flags;
    uint16_t status;
    uint16_t payload_len;
    uint16_t header_crc16;
    uint16_t reserved;
} ch32mux_header_t;

typedef struct {
    uint8_t version;
    uint8_t urgent_flags;
    uint16_t pending_bitmap;
    uint16_t dropped_bitmap;
    uint16_t reserved;
} ch32mux_irq_hint_t;

typedef struct {
    uint8_t proto_version;
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
    uint8_t uart_port_count;
    uint8_t ble_max_links;
    uint8_t reserved0;
    uint8_t reserved1;
} ch32mux_dev_info_t;

typedef struct {
    uint32_t caps_bitmap;
    uint8_t uart_port_count;
    uint8_t ble_max_links;
    uint8_t net_reserved;
    uint8_t reserved0;
} ch32mux_caps_t;

uint16_t ch32mux_crc16(const uint8_t *data, size_t len);
int ch32mux_encode_header(uint8_t *dst, size_t dst_len, ch32mux_header_t *header);
int ch32mux_decode_header(const uint8_t *src, size_t src_len, ch32mux_header_t *header);
int ch32mux_build_frame(uint8_t *dst,
                        size_t dst_len,
                        size_t *frame_len,
                        ch32mux_header_t *header,
                        const uint8_t *payload,
                        size_t payload_len);
int ch32mux_parse_irq_hint(const uint8_t *src, size_t src_len, ch32mux_irq_hint_t *hint);
const char *ch32mux_result_name(int result);
const char *ch32mux_device_status_name(uint16_t status);

#ifdef __cplusplus
}
#endif

#endif

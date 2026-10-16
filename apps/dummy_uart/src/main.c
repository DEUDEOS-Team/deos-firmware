#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <deos/deos_types.h>
#include <deos/deos_frame.h>
#include <zephyr/sys/crc.h>
#include <string.h>

LOG_MODULE_REGISTER(dummy_lora, LOG_LEVEL_INF);

/* Framing Constants */
#define LORA_MAGIC_1       0xAA
#define LORA_MAGIC_2       0x55
#define LORA_FLAG_NONE     0x00
#define LORA_FLAG_ACK_REQ  0x01
#define LORA_FLAG_IS_ACK   0x02
#define LORA_MAX_PAYLOAD   100

#define RING_BUF_SIZE 1024
RING_BUF_DECLARE(lora_rx_rb, RING_BUF_SIZE);

K_MSGQ_DEFINE(ack_q, sizeof(uint16_t), 10, 4);

static const struct device *uart_dev;

static void uart_rx_cb(const struct device *dev, void *user_data)
{
    uint8_t c;
    uart_irq_update(dev);
    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            ring_buf_put(&lora_rx_rb, &c, 1);
        }
    }
}

/* Helper to send a frame and return its calculated CRC */
static uint16_t send_lora_frame(uint8_t flag, const uint8_t *payload, uint8_t len)
{
    uint8_t header[4] = { LORA_MAGIC_1, LORA_MAGIC_2, flag, len };
    uint16_t crc = crc16_ccitt(0xFFFF, &flag, 1);
    crc = crc16_ccitt(crc, &len, 1);
    if (len > 0) crc = crc16_ccitt(crc, payload, len);
    
    for(int i = 0; i < 4; i++) uart_poll_out(uart_dev, header[i]);
    for(int i = 0; i < len; i++) uart_poll_out(uart_dev, payload[i]);
    uart_poll_out(uart_dev, crc & 0xFF);
    uart_poll_out(uart_dev, (crc >> 8) & 0xFF);
    
    return crc;
}

typedef enum {
    STATE_WAIT_MAGIC1, STATE_WAIT_MAGIC2,
    STATE_WAIT_FLAG, STATE_WAIT_LENGTH,
    STATE_WAIT_PAYLOAD, STATE_WAIT_CRC1, STATE_WAIT_CRC2
} rx_state_t;

static void dummy_rx_thread(void *p1, void *p2, void *p3)
{
    LOG_INF("Dummy RX Thread started with Framing and ACK Support!");
    
    rx_state_t rx_state = STATE_WAIT_MAGIC1;
    uint8_t rx_len = 0, rx_flag = 0;
    uint8_t rx_buf[LORA_MAX_PAYLOAD];
    int rx_idx = 0;
    uint16_t rx_crc = 0;

    while (1) {
        uint8_t c;
        if (ring_buf_get(&lora_rx_rb, &c, 1) == 1) {
            switch(rx_state) {
                case STATE_WAIT_MAGIC1:
                    if (c == LORA_MAGIC_1) rx_state = STATE_WAIT_MAGIC2;
                    break;
                case STATE_WAIT_MAGIC2:
                    if (c == LORA_MAGIC_2) rx_state = STATE_WAIT_FLAG;
                    else if (c != LORA_MAGIC_1) rx_state = STATE_WAIT_MAGIC1;
                    break;
                case STATE_WAIT_FLAG:
                    rx_flag = c;
                    rx_state = STATE_WAIT_LENGTH;
                    break;
                case STATE_WAIT_LENGTH:
                    rx_len = c;
                    rx_idx = 0;
                    if (rx_len > LORA_MAX_PAYLOAD) {
                        rx_state = STATE_WAIT_MAGIC1;
                    } else if (rx_len == 0) {
                        rx_state = STATE_WAIT_CRC1;
                    } else {
                        rx_state = STATE_WAIT_PAYLOAD;
                    }
                    break;
                case STATE_WAIT_PAYLOAD:
                    rx_buf[rx_idx++] = c;
                    if (rx_idx >= rx_len) rx_state = STATE_WAIT_CRC1;
                    break;
                case STATE_WAIT_CRC1:
                    rx_crc = c;
                    rx_state = STATE_WAIT_CRC2;
                    break;
                case STATE_WAIT_CRC2:
                    rx_crc |= (uint16_t)c << 8;
                    
                    uint16_t calc_crc = crc16_ccitt(0xFFFF, &rx_flag, 1);
                    calc_crc = crc16_ccitt(calc_crc, &rx_len, 1);
                    if (rx_len > 0) calc_crc = crc16_ccitt(calc_crc, rx_buf, rx_len);
                    
                    if (calc_crc == rx_crc) {
                        if (rx_flag == LORA_FLAG_IS_ACK && rx_len == 2) {
                            uint16_t acked_crc = rx_buf[0] | (rx_buf[1] << 8);
                            k_msgq_put(&ack_q, &acked_crc, K_NO_WAIT);
                        } else if (rx_flag == LORA_FLAG_ACK_REQ) {
                            /* Loopback mode: immediately send ACK back */
                            uint8_t ack_payload[2] = { calc_crc & 0xFF, (calc_crc >> 8) & 0xFF };
                            send_lora_frame(LORA_FLAG_IS_ACK, ack_payload, 2);
                        } else {
                            if (rx_len >= 9) {
                                uint8_t cmd = rx_buf[6];
                                uint8_t src = rx_buf[1];
                                LOG_INF("<<< RX Universal Frame (Len: %d, Src: 0x%02X, Cmd: 0x%02X)", rx_len, src, cmd);
                            }
                        }
                    } else {
                        LOG_WRN("CRC mismatch! Calc: 0x%04X, Rx: 0x%04X", calc_crc, rx_crc);
                    }
                    rx_state = STATE_WAIT_MAGIC1;
                    break;
            }
        } else {
            k_msleep(1);
        }
    }
}

K_THREAD_DEFINE(dummy_rx_tid, 1024, dummy_rx_thread, NULL, NULL, NULL, 7, 0, 0);

int main(void)
{
    uart_dev = DEVICE_DT_GET(DT_ALIAS(lora_uart));

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return 0;
    }

    uart_irq_callback_user_data_set(uart_dev, uart_rx_cb, NULL);
    uart_irq_rx_enable(uart_dev);

    LOG_INF("Dummy LoRa Ground Control starting (With Retry & ACK)...");
    LOG_INF("Using UART TX on PG14 (D1) and RX on PG9 (D0)");
    
    deos_message_t msg;
    uint32_t ping_id = 1;
    uint8_t seq = 0;

    while (1) {
        memset(&msg, 0, sizeof(msg));
        
        msg.priority = DEOS_PRIO_NETWORK;
        msg.message_class = DEOS_CLASS_NETWORK;
        msg.service = DEOS_SERVICE_SYSTEM;
        msg.destination = DEOS_NODE_TEXTUAL;
        msg.source = DEOS_NODE_GROUND_CONTROL;
        
        msg.version = DEOS_PROTOCOL_VERSION;
        msg.sequence = seq++;
        msg.command = DEOS_CMD_SYSTEM_PING;
        msg.payload_len = 4;
        
        memcpy(msg.payload, &ping_id, 4);

        uint8_t frame_buf[LORA_MAX_PAYLOAD];
        int send_len = deos_frame_encode(&msg, frame_buf, sizeof(frame_buf));
        if (send_len < 0) {
            LOG_ERR("Failed to encode universal frame: %d", send_len);
            continue;
        }

        int retries = 3;
        bool ack_received = false;
        
        while (retries-- > 0 && !ack_received) {
            LOG_INF(">>> Sending PING %u (Attempt %d/3, Len: %d)", ping_id, 3 - retries, send_len);
            uint16_t sent_crc = send_lora_frame(LORA_FLAG_ACK_REQ, frame_buf, send_len);
            
            /* Wait for ACK up to 500ms */
            uint16_t rcv_crc;
            if (k_msgq_get(&ack_q, &rcv_crc, K_MSEC(500)) == 0) {
                if (rcv_crc == sent_crc) {
                    LOG_INF("<<< PING %u Delivered! (ACK Received for CRC 0x%04X)", ping_id, rcv_crc);
                    ack_received = true;
                }
            } else {
                LOG_WRN("Timeout waiting for ACK...");
            }
        }
        
        if (!ack_received) {
            LOG_ERR("PING %u FAILED! (No ACK after 3 attempts)", ping_id);
        }

        ping_id++;
        k_msleep(2000);
    }
    return 0;
}

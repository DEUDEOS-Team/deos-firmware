#include "uart_lora.hpp"
#include <deos/deos_frame.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/crc.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>

#define LORA_AUX_NODE DT_NODELABEL(lora_aux)
static const struct gpio_dt_spec aux_pin = GPIO_DT_SPEC_GET(LORA_AUX_NODE, gpios);

LOG_MODULE_REGISTER(uart_lora, LOG_LEVEL_INF);

#define LORA_TX_QUEUE_SIZE 16
#define LORA_THREAD_STACK_SIZE 1024
#define LORA_THREAD_PRIO 5
#define RING_BUF_SIZE 1024

/* Framing Constants */
#define LORA_MAGIC_1       0xAA
#define LORA_MAGIC_2       0x55
#define LORA_FLAG_NONE     0x00
#define LORA_FLAG_ACK_REQ  0x01
#define LORA_FLAG_IS_ACK   0x02
#define LORA_MAX_PAYLOAD   100

static K_KERNEL_STACK_DEFINE(lora_rx_stack, LORA_THREAD_STACK_SIZE);
static K_KERNEL_STACK_DEFINE(lora_tx_stack, LORA_THREAD_STACK_SIZE);
static struct k_thread lora_rx_thread_data;
static struct k_thread lora_tx_thread_data;

UartLora& UartLora::getInstance() {
    static UartLora instance;
    return instance;
}

UartLora::UartLora() : m_uart_dev(DEVICE_DT_GET(DT_ALIAS(lora_uart))) {
    ring_buf_init(&m_rx_rb, RING_BUF_SIZE, m_rx_rb_buffer);
    k_msgq_init(&m_tx_q, m_tx_q_buffer, sizeof(deos_message_t), LORA_TX_QUEUE_SIZE);
    k_sem_init(&m_rx_sem, 0, K_SEM_MAX_LIMIT);
    k_mutex_init(&m_tx_lock);
}

int UartLora::tx_callback(const deos_message_t *msg) {
    return getInstance().send(msg);
}

int UartLora::send(const deos_message_t *msg) {
    if (k_msgq_put(&m_tx_q, msg, K_NO_WAIT) != 0) {
        LOG_WRN("LoRa TX queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_LORA_TX_QUEUE_FULL, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}

void UartLora::uart_rx_cb(const struct device *dev, void *user_data) {
    UartLora* instance = static_cast<UartLora*>(user_data);
    uint8_t c;

    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (ring_buf_put(&instance->m_rx_rb, &c, 1) == 0) {
                // Overflow
            }
            k_sem_give(&instance->m_rx_sem);
        }
    }

    int err = uart_err_check(dev);
    if (err) {
        LOG_WRN("UART Error Cleared: %d", err);
    }
}

static uint16_t deos_crc16_ccitt(uint16_t seed, const uint8_t *src, size_t len) {
    for (; len > 0; len--) {
        uint8_t e = *src++;
        seed = seed ^ ((uint16_t)e << 8);
        for (int i = 0; i < 8; i++) {
            if (seed & 0x8000) {
                seed = (seed << 1) ^ 0x1021;
            } else {
                seed = seed << 1;
            }
        }
    }
    return seed;
}

void UartLora::send_lora_frame(uint8_t flag, const uint8_t *payload, uint8_t len) {
    k_mutex_lock(&m_tx_lock, K_FOREVER);

    uint8_t header[4] = { LORA_MAGIC_1, LORA_MAGIC_2, flag, len };
    uint16_t crc = deos_crc16_ccitt(0xFFFF, &flag, 1);
    crc = deos_crc16_ccitt(crc, &len, 1);
    if (len > 0 && payload != nullptr) crc = deos_crc16_ccitt(crc, payload, len);
    
    for(int i=0; i<4; i++) uart_poll_out(m_uart_dev, header[i]);
    for(int i=0; i<len; i++) uart_poll_out(m_uart_dev, payload[i]);
    uart_poll_out(m_uart_dev, crc & 0xFF);
    uart_poll_out(m_uart_dev, (crc >> 8) & 0xFF);

    k_mutex_unlock(&m_tx_lock);
}

void UartLora::send_deos_lora(uint8_t flag, const deos_message_t *msg) {
    uint8_t buf[LORA_MAX_PAYLOAD];
    int encoded_len = deos_frame_encode(msg, buf, sizeof(buf));
    if (encoded_len > 0) {
        send_lora_frame(flag, buf, static_cast<uint8_t>(encoded_len));
    } else {
        LOG_ERR("Failed to encode universal frame: %d", encoded_len);
    }
}

void UartLora::tx_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<UartLora*>(p1);
    instance->tx_thread();
}

void UartLora::tx_thread() {
    deos_message_t msg;
    LOG_INF("LoRa TX Thread started with Dynamic Framing (OOP)");

    while (1) {
        if (k_msgq_get(&m_tx_q, &msg, K_FOREVER) == 0) {
            send_deos_lora(LORA_FLAG_NONE, &msg);
            LOG_DBG("LoRa TX: [%s (0x%02X) -> %s (0x%02X)] Class: %s, Svc: %s, Cmd: 0x%02X, Len: %d", 
                    deos_node_name(msg.source), msg.source,
                    deos_node_name(msg.destination), msg.destination,
                    deos_class_name(msg.message_class),
                    deos_service_name(msg.service),
                    msg.command, msg.payload_len);
        }
    }
}

typedef enum {
    STATE_WAIT_MAGIC1, STATE_WAIT_MAGIC2,
    STATE_WAIT_FLAG, STATE_WAIT_LENGTH,
    STATE_WAIT_PAYLOAD, STATE_WAIT_CRC1, STATE_WAIT_CRC2
} rx_state_t;

void UartLora::rx_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<UartLora*>(p1);
    instance->rx_thread();
}

void UartLora::rx_thread() {
    LOG_INF("LoRa RX Thread started with Framing and ACK Support! (OOP)");
    
    rx_state_t rx_state = STATE_WAIT_MAGIC1;
    uint8_t rx_len = 0, rx_flag = 0;
    uint8_t rx_buf[LORA_MAX_PAYLOAD];
    int rx_idx = 0;
    uint16_t rx_crc = 0;

    while (1) {
        uint8_t c;
        if (ring_buf_get(&m_rx_rb, &c, 1) == 1) {
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
                    
                    uint16_t calc_crc = deos_crc16_ccitt(0xFFFF, &rx_flag, 1);
                    calc_crc = deos_crc16_ccitt(calc_crc, &rx_len, 1);
                    if (rx_len > 0) calc_crc = deos_crc16_ccitt(calc_crc, rx_buf, rx_len);
                    
                    if (calc_crc == rx_crc) {
                        if (rx_flag == LORA_FLAG_ACK_REQ) {
                            uint8_t ack_payload[2] = { static_cast<uint8_t>(calc_crc & 0xFF), static_cast<uint8_t>((calc_crc >> 8) & 0xFF) };
                            send_lora_frame(LORA_FLAG_IS_ACK, ack_payload, 2);
                            LOG_DBG("LoRa ACK TX: Gelen pakete ACK yollandi (CRC 0x%04X)", calc_crc);
                        }
                        
                        if (rx_flag != LORA_FLAG_IS_ACK) {
                            if (rx_len >= DEOS_UNIVERSAL_HEADER_SIZE) {
                                deos_feed_frame(rx_buf, rx_len, DEOS_TRANSPORT_UART_LORA);
                            } else {
                                LOG_WRN("LoRa [HATA]: Havadan gelen paket DEOS basligi icin cok kucuk! (len: %d)", rx_len);
                                deos_fault_raise(DEOS_FAULT_LORA_FRAME_MALFORMED, DEOS_FAULT_SEVERITY_WARNING);
                            }
                        }
                    } else {
                        LOG_WRN("LoRa [HATA - CRC]: Havadan gelen paket bozuk! Calc: 0x%04X, Rx: 0x%04X", calc_crc, rx_crc);
                        LOG_WRN("Flag: %02X, Len: %02X", rx_flag, rx_len);
                        for (int i=0; i<rx_len; i++) {
                            LOG_WRN("Data[%d]: %02X", i, rx_buf[i]);
                        }
                        deos_fault_raise(DEOS_FAULT_LORA_CRC_MISMATCH, DEOS_FAULT_SEVERITY_WARNING);
                    }
                    rx_state = STATE_WAIT_MAGIC1;
                    break;
            }
        } else {
            k_sem_take(&m_rx_sem, K_FOREVER);
        }
    }
}

int UartLora::init() {
    if (!device_is_ready(m_uart_dev)) {
        LOG_ERR("LoRa UART device not ready!");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&aux_pin)) {
        LOG_WRN("LoRa AUX pin not ready");
    } else {
        // M0 ve M1 donanimsal olarak GND'ye bagli oldugu icin konfigure etmiyoruz.
        // AUX pini DT uzerinden konfigure ediliyor
        gpio_pin_configure_dt(&aux_pin, GPIO_INPUT);
    }

    uart_irq_callback_user_data_set(m_uart_dev, uart_rx_cb, this);
    uart_irq_rx_enable(m_uart_dev);
    uart_irq_err_enable(m_uart_dev);

    deos_router_register_transport(DEOS_TRANSPORT_UART_LORA, tx_callback);

    k_thread_create(&lora_tx_thread_data, lora_tx_stack,
                    K_KERNEL_STACK_SIZEOF(lora_tx_stack),
                    tx_thread_entry, this, NULL, NULL,
                    LORA_THREAD_PRIO, 0, K_NO_WAIT);

    k_thread_create(&lora_rx_thread_data, lora_rx_stack,
                    K_KERNEL_STACK_SIZEOF(lora_rx_stack),
                    rx_thread_entry, this, NULL, NULL,
                    LORA_THREAD_PRIO, 0, K_NO_WAIT);

    LOG_INF("LoRa UART interface initialized");
    return 0;
}

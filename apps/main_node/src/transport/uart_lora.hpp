#pragma once

#include "../ITransport.hpp"
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/device.h>

class UartLora : public ITransport {
public:
    static UartLora& getInstance();

    int init() override;
    int send(const deos_message_t* msg) override;

private:
    UartLora();
    ~UartLora() = default;

    // Non-copyable
    UartLora(const UartLora&) = delete;
    UartLora& operator=(const UartLora&) = delete;

    void tx_thread();
    void rx_thread();
    
    void send_lora_frame(uint8_t flag, const uint8_t *payload, uint8_t len);
    void send_deos_lora(uint8_t flag, const deos_message_t *msg);

    static void tx_thread_entry(void *p1, void *p2, void *p3);
    static void rx_thread_entry(void *p1, void *p2, void *p3);
    static void uart_rx_cb(const struct device *dev, void *user_data);
    static int tx_callback(const deos_message_t *msg);

    const struct device *m_uart_dev;
    
    struct ring_buf m_rx_rb;
    uint8_t m_rx_rb_buffer[1024];

    struct k_msgq m_tx_q;
    char m_tx_q_buffer[16 * sizeof(deos_message_t)]; // LORA_TX_QUEUE_SIZE = 16

    struct k_sem m_rx_sem;
    struct k_mutex m_tx_lock;
};

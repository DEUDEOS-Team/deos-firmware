#include "eth_textual.hpp"
#include "universal_frame.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>

LOG_MODULE_REGISTER(eth_textual, LOG_LEVEL_INF);

#define TEXTUAL_TX_QUEUE_SIZE 16
#define TEXTUAL_THREAD_STACK_SIZE 2048
#define TEXTUAL_THREAD_PRIO 5
#define TEXTUAL_TCP_PORT 5000
#define TEXTUAL_MAGIC_1  0xAA
#define TEXTUAL_MAGIC_2  0x55

/* Global threads and stacks since we cannot easily put them in the class without exposing Kconfig macros */
static K_KERNEL_STACK_DEFINE(textual_server_stack, TEXTUAL_THREAD_STACK_SIZE);
static K_KERNEL_STACK_DEFINE(textual_tx_stack, TEXTUAL_THREAD_STACK_SIZE);
static struct k_thread textual_server_thread_data;
static struct k_thread textual_tx_thread_data;

EthTextual& EthTextual::getInstance() {
    static EthTextual instance;
    return instance;
}

EthTextual::EthTextual() : m_active_client_sock(-1) {
    k_msgq_init(&m_tx_q, m_tx_q_buffer, sizeof(deos_message_t), TEXTUAL_TX_QUEUE_SIZE);
}

int EthTextual::tx_callback(const deos_message_t *msg) {
    return getInstance().send(msg);
}

int EthTextual::send(const deos_message_t *msg) {
    if (m_active_client_sock < 0) {
        return -ENOTCONN;
    }

    if (k_msgq_put(&m_tx_q, msg, K_NO_WAIT) != 0) {
        LOG_WRN("Textual TX queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}

void EthTextual::tx_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<EthTextual*>(p1);
    instance->tx_thread();
}

void EthTextual::tx_thread() {
    deos_message_t msg;
    LOG_INF("Textual TCP TX Thread started");

    while (1) {
        if (k_msgq_get(&m_tx_q, &msg, K_FOREVER) == 0) {
            if (m_active_client_sock >= 0) {
                uint8_t tx_buf[DEOS_MAX_PAYLOAD_LEN + 30];
                tx_buf[0] = TEXTUAL_MAGIC_1;
                tx_buf[1] = TEXTUAL_MAGIC_2;
                int encoded_len = deos_universal_frame_encode(&msg, tx_buf + 3, sizeof(tx_buf) - 3);
                if (encoded_len > 0) {
                    tx_buf[2] = static_cast<uint8_t>(encoded_len);
                    int sent = zsock_send(m_active_client_sock, tx_buf, 3 + encoded_len, 0);
                    if (sent < 0) {
                        LOG_ERR("TCP Send failed (%d), dropping client", errno);
                        zsock_close(m_active_client_sock);
                        m_active_client_sock = -1;
                    } else {
                        LOG_INF("Textual UI TX: [%s (0x%02X) -> %s (0x%02X)] Class: %s, Svc: %s, Cmd: 0x%02X, Len: %d",
                                deos_node_name(msg.source), msg.source,
                                deos_node_name(msg.destination), msg.destination,
                                deos_class_name(msg.message_class),
                                deos_service_name(msg.service),
                                msg.command, msg.payload_len);
                    }
                } else {
                    LOG_ERR("Failed to encode universal frame for Textual: %d", encoded_len);
                }
            }
        }
    }
}

void EthTextual::server_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<EthTextual*>(p1);
    instance->server_thread();
}

void EthTextual::server_thread() {
    int serv_sock;
    struct sockaddr_in bind_addr;

    serv_sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock < 0) {
        LOG_ERR("Failed to create TCP socket: %d", errno);
        return;
    }

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(TEXTUAL_TCP_PORT);

    if (zsock_bind(serv_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        LOG_ERR("Failed to bind TCP socket: %d", errno);
        zsock_close(serv_sock);
        return;
    }

    if (zsock_listen(serv_sock, 1) < 0) {
        LOG_ERR("Failed to listen on TCP socket: %d", errno);
        zsock_close(serv_sock);
        return;
    }

    LOG_INF("Textual TCP Server listening on port %d", TEXTUAL_TCP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        LOG_INF("Waiting for Textual UI connection...");
        int client = zsock_accept(serv_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (client < 0) {
            LOG_ERR("Accept failed: %d", errno);
            k_msleep(1000);
            continue;
        }

        LOG_INF("Textual UI Connected!");
        m_active_client_sock = client;

        typedef enum {
            STATE_WAIT_MAGIC_1,
            STATE_WAIT_MAGIC_2,
            STATE_WAIT_LEN
        } tcp_rx_state_t;

        tcp_rx_state_t rx_state = STATE_WAIT_MAGIC_1;
        uint8_t rx_len = 0;

        while (1) {
            uint8_t byte_in;
            int ret = zsock_recv(client, &byte_in, 1, 0);
            if (ret <= 0) {
                LOG_INF("Textual UI Disconnected.");
                break;
            }

            switch (rx_state) {
            case STATE_WAIT_MAGIC_1:
                if (byte_in == TEXTUAL_MAGIC_1) {
                    rx_state = STATE_WAIT_MAGIC_2;
                }
                break;

            case STATE_WAIT_MAGIC_2:
                if (byte_in == TEXTUAL_MAGIC_2) {
                    rx_state = STATE_WAIT_LEN;
                } else if (byte_in != TEXTUAL_MAGIC_1) {
                    rx_state = STATE_WAIT_MAGIC_1;
                }
                break;

            case STATE_WAIT_LEN: {
                rx_len = byte_in;

                if (rx_len < DEOS_UNIVERSAL_HEADER_SIZE || rx_len > DEOS_MAX_PAYLOAD_LEN + DEOS_UNIVERSAL_HEADER_SIZE) {
                    LOG_WRN("Invalid frame length %d received after magic word.", rx_len);
                    rx_state = STATE_WAIT_MAGIC_1;
                    break;
                }

                uint8_t frame_buf[DEOS_MAX_PAYLOAD_LEN + DEOS_UNIVERSAL_HEADER_SIZE + 10];
                int received = 0;
                bool client_err = false;

                while (received < rx_len) {
                    ret = zsock_recv(client, frame_buf + received, rx_len - received, 0);
                    if (ret <= 0) {
                        client_err = true;
                        break;
                    }
                    received += ret;
                }

                if (client_err) {
                    LOG_INF("Textual UI Disconnected during payload read.");
                    goto client_cleanup;
                }

                if (received == rx_len) {
                    deos_message_t msg;
                    int decode_ret = deos_universal_frame_decode(frame_buf, rx_len, &msg);
                    if (decode_ret == 0) {
                        LOG_INF("Textual UI RX: [%s (0x%02X) -> %s (0x%02X)] Class: %s, Svc: %s, Cmd: 0x%02X, Len: %d -> Router'a iletiliyor",
                                deos_node_name(msg.source), msg.source,
                                deos_node_name(msg.destination), msg.destination,
                                deos_class_name(msg.message_class),
                                deos_service_name(msg.service),
                                msg.command, msg.payload_len);
                        deos_feed_message(&msg, DEOS_TRANSPORT_ETH_TEXTUAL);
                    } else {
                        LOG_WRN("Failed to decode Textual universal frame: %d", decode_ret);
                    }
                }

                rx_state = STATE_WAIT_MAGIC_1;
                break;
            }
            }
        }

client_cleanup:
        zsock_close(client);
        m_active_client_sock = -1;
    }
}

int EthTextual::init() {
    deos_router_register_transport(DEOS_TRANSPORT_ETH_TEXTUAL, tx_callback);

    k_thread_create(&textual_tx_thread_data, textual_tx_stack,
                    K_KERNEL_STACK_SIZEOF(textual_tx_stack),
                    tx_thread_entry, this, NULL, NULL,
                    TEXTUAL_THREAD_PRIO, 0, K_NO_WAIT);

    k_thread_create(&textual_server_thread_data, textual_server_stack,
                    K_KERNEL_STACK_SIZEOF(textual_server_stack),
                    server_thread_entry, this, NULL, NULL,
                    TEXTUAL_THREAD_PRIO, 0, K_NO_WAIT);

    LOG_INF("Textual Ethernet interface initialized");
    return 0;
}

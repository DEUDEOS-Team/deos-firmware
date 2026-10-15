#include "eth_ros_bridge.hpp"
#include "universal_frame.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>

LOG_MODULE_REGISTER(eth_ros_bridge, LOG_LEVEL_INF);

#define ROS_BRIDGE_TX_QUEUE_SIZE 16
#define ROS_BRIDGE_THREAD_STACK_SIZE 2048
#define ROS_BRIDGE_THREAD_PRIO 5
#define ROS_BRIDGE_UDP_PORT 5001

static K_KERNEL_STACK_DEFINE(ros_bridge_rx_stack, ROS_BRIDGE_THREAD_STACK_SIZE);
static K_KERNEL_STACK_DEFINE(ros_bridge_tx_stack, ROS_BRIDGE_THREAD_STACK_SIZE);
static struct k_thread ros_bridge_rx_thread_data;
static struct k_thread ros_bridge_tx_thread_data;

EthRosBridge& EthRosBridge::getInstance() {
    static EthRosBridge instance;
    return instance;
}

EthRosBridge::EthRosBridge() : m_sock(-1), m_client_connected(false) {
    k_msgq_init(&m_tx_q, m_tx_q_buffer, sizeof(deos_message_t), ROS_BRIDGE_TX_QUEUE_SIZE);
    memset(&m_client_addr, 0, sizeof(m_client_addr));
}

int EthRosBridge::tx_callback(const deos_message_t *msg) {
    return getInstance().send(msg);
}

int EthRosBridge::send(const deos_message_t *msg) {
    if (k_msgq_put(&m_tx_q, msg, K_NO_WAIT) != 0) {
        LOG_WRN("ROS Bridge TX queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}

void EthRosBridge::tx_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<EthRosBridge*>(p1);
    instance->tx_thread();
}

void EthRosBridge::tx_thread() {
    deos_message_t msg;
    LOG_INF("ROS Bridge UDP TX Thread started");

    while (1) {
        if (k_msgq_get(&m_tx_q, &msg, K_FOREVER) == 0) {
            if (m_sock >= 0 && m_client_connected) {
                uint8_t tx_buf[DEOS_MAX_PAYLOAD_LEN + 30];
                int encoded_len = deos_universal_frame_encode(&msg, tx_buf + 1, sizeof(tx_buf) - 1);
                if (encoded_len > 0) {
                    tx_buf[0] = static_cast<uint8_t>(encoded_len);
                    int sent = zsock_sendto(m_sock, tx_buf, encoded_len + 1, 0,
                                            (struct sockaddr *)&m_client_addr,
                                            sizeof(m_client_addr));
                    if (sent < 0) {
                        LOG_ERR("UDP Send failed (%d)", errno);
                    }
                } else {
                    LOG_ERR("Failed to encode universal frame for ROS bridge: %d", encoded_len);
                }
            }
        }
    }
}

void EthRosBridge::rx_thread_entry(void *p1, void *p2, void *p3) {
    auto instance = static_cast<EthRosBridge*>(p1);
    instance->rx_thread();
}

void EthRosBridge::rx_thread() {
    struct sockaddr_in bind_addr;

    m_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock < 0) {
        LOG_ERR("Failed to create UDP socket: %d", errno);
        return;
    }

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(ROS_BRIDGE_UDP_PORT);

    if (zsock_bind(m_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        LOG_ERR("Failed to bind UDP socket: %d", errno);
        zsock_close(m_sock);
        m_sock = -1;
        return;
    }

    LOG_INF("ROS Bridge UDP Server listening on port %d", ROS_BRIDGE_UDP_PORT);

    while (1) {
        uint8_t rx_buf[DEOS_MAX_PAYLOAD_LEN + 30];
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int received = zsock_recvfrom(m_sock, rx_buf, sizeof(rx_buf), 0,
                                      (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (received < 0) {
            LOG_ERR("UDP Recv failed: %d", errno);
            k_msleep(1000);
            continue;
        }

        if (!m_client_connected) {
            m_client_addr = client_addr;
            m_client_connected = true;
            LOG_INF("ROS Bridge UDP Client discovered!");
        } else if (m_client_addr.sin_addr.s_addr != client_addr.sin_addr.s_addr || 
                   m_client_addr.sin_port != client_addr.sin_port) {
            // Update client address if a new one sends data
            m_client_addr = client_addr;
            LOG_INF("ROS Bridge UDP Client updated.");
        }

        uint8_t rx_len = rx_buf[0];
        if (rx_len < DEOS_UNIVERSAL_HEADER_SIZE || rx_len > sizeof(rx_buf) - 1 || received != rx_len + 1) {
            LOG_WRN("Invalid UDP frame received.");
            continue;
        }

        deos_message_t msg;
        int ret = deos_universal_frame_decode(rx_buf + 1, rx_len, &msg);
        if (ret == 0) {
            deos_feed_message(&msg, DEOS_TRANSPORT_ETH_ROS_BRIDGE);
        } else {
            LOG_WRN("Failed to decode UDP universal frame: %d", ret);
        }
    }
}

int EthRosBridge::init() {
    deos_router_register_transport(DEOS_TRANSPORT_ETH_ROS_BRIDGE, tx_callback);

    k_thread_create(&ros_bridge_tx_thread_data, ros_bridge_tx_stack,
                    K_KERNEL_STACK_SIZEOF(ros_bridge_tx_stack),
                    tx_thread_entry, this, NULL, NULL,
                    ROS_BRIDGE_THREAD_PRIO, 0, K_NO_WAIT);

    k_thread_create(&ros_bridge_rx_thread_data, ros_bridge_rx_stack,
                    K_KERNEL_STACK_SIZEOF(ros_bridge_rx_stack),
                    rx_thread_entry, this, NULL, NULL,
                    ROS_BRIDGE_THREAD_PRIO, 0, K_NO_WAIT);

    LOG_INF("ROS Bridge UDP interface initialized");
    return 0;
}

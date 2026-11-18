#pragma once

#include "../ITransport.hpp"
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

class EthRosBridge : public ITransport {
public:
    static EthRosBridge& getInstance();

    int init() override;
    int send(const deos_message_t* msg) override;

private:
    EthRosBridge();
    ~EthRosBridge() = default;

    // Non-copyable
    EthRosBridge(const EthRosBridge&) = delete;
    EthRosBridge& operator=(const EthRosBridge&) = delete;

    void tx_thread();
    void rx_thread();

    static void tx_thread_entry(void *p1, void *p2, void *p3);
    static void rx_thread_entry(void *p1, void *p2, void *p3);
    static int tx_callback(const deos_message_t *msg);

    int m_sock;
    struct sockaddr_in m_bridge_addr{};

    struct k_msgq m_tx_q;
    char m_tx_q_buffer[16 * sizeof(deos_message_t)]; // ROS_BRIDGE_TX_QUEUE_SIZE = 16
};

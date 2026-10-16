#include <deos/deos_can.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_can_trp, LOG_LEVEL_INF);

#define DEOS_CAN_TX_QUEUE_SIZE 16
#define DEOS_CAN_RX_QUEUE_SIZE 16
#define DEOS_CAN_THREAD_STACK_SIZE 1024
#define DEOS_CAN_THREAD_PRIORITY 5
#define DEOS_CAN_MAX_RX_FILTERS 8

static const struct device *m_can_dev = NULL;
static bool m_is_initialized = false;
static bool m_is_started = false;

/* Queues */
K_MSGQ_DEFINE(can_tx_msgq, sizeof(deos_message_t), DEOS_CAN_TX_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(can_rx_msgq, sizeof(struct can_frame), DEOS_CAN_RX_QUEUE_SIZE, 4);

/* Stacks & Threads */
static struct k_thread can_tx_thread_data;
static K_KERNEL_STACK_DEFINE(can_tx_thread_stack, DEOS_CAN_THREAD_STACK_SIZE);

static struct k_thread can_rx_thread_data;
static K_KERNEL_STACK_DEFINE(can_rx_thread_stack, DEOS_CAN_THREAD_STACK_SIZE);

static int rx_filter_ids[DEOS_CAN_MAX_RX_FILTERS];
static atomic_t rx_overflow_flag = ATOMIC_INIT(0);

/* ISR Callback */
static void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    if (k_msgq_put(&can_rx_msgq, frame, K_NO_WAIT) != 0) {
        atomic_set(&rx_overflow_flag, 1);
    }
}

/* RX Worker Thread */
static void can_rx_thread_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct can_frame frame;
    deos_message_t msg;

    LOG_INF("DEOS CAN-FD RX Thread started");

    while (1) {
        if (atomic_cas(&rx_overflow_flag, 1, 0)) {
            deos_fault_raise(DEOS_FAULT_RX_QUEUE_OVERFLOW, DEOS_FAULT_SEVERITY_ERROR);
        }

        if (k_msgq_get(&can_rx_msgq, &frame, K_FOREVER) == 0) {
            int ret = deos_can_decode_frame(&frame, &msg);
            if (ret != 0) {
                LOG_WRN("Failed to decode CAN frame: %d", ret);
                deos_fault_raise(DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
                continue;
            }

            /* Feed received message into core via DEOS_TRANSPORT_CAN_FD */
            deos_feed_message(&msg, DEOS_TRANSPORT_CAN_FD);
        }
    }
}

/* TX Worker Thread */
static void can_tx_thread_func(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    deos_message_t msg;
    LOG_INF("DEOS CAN-FD TX Thread started");

    while (1) {
        if (k_msgq_get(&can_tx_msgq, &msg, K_FOREVER) == 0) {
            if (m_can_dev) {
                struct can_frame frame = {0};
                if (deos_can_encode_frame(&msg, &frame) == 0) {
                    int ret = can_send(m_can_dev, &frame, K_MSEC(100), NULL, NULL);
                    if (ret != 0) {
                        LOG_ERR("CAN send failed: %d", ret);
                        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
                    } else {
                        LOG_DBG("CAN Sent MSG (Cmd: 0x%02X) Seq: %u", msg.command, msg.sequence);
                    }
                }
            }
        }
    }
}

int deos_can_transport_tx(const deos_message_t *msg)
{
    if (!msg) {
        return -EINVAL;
    }

    if (!m_is_initialized || !m_can_dev) {
        return -ENODEV;
    }

    if (k_msgq_put(&can_tx_msgq, msg, K_NO_WAIT) != 0) {
        LOG_WRN("CAN TX Queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }

    return 0;
}

int deos_can_transport_init(const struct device *can_dev)
{
    if (!can_dev) {
        return -EINVAL;
    }

    if (!device_is_ready(can_dev)) {
        LOG_ERR("CAN device %s is not ready", can_dev->name);
        return -ENODEV;
    }

    m_can_dev = can_dev;

    for (int i = 0; i < DEOS_CAN_MAX_RX_FILTERS; i++) {
        rx_filter_ids[i] = -1;
    }

    int ret = can_set_mode(m_can_dev, CAN_MODE_FD);
    if (ret != 0 && ret != -ENOTSUP) {
        LOG_ERR("Failed to set CAN-FD mode: %d", ret);
        return ret;
    }

    if (ret == -ENOTSUP) {
        LOG_ERR("Hardware does not support CAN-FD mode!");
        return ret;
    }

    /* Check core configuration to decide filter mode */
    const struct deos_config *core_cfg = deos_get_config();
    bool is_router = (core_cfg != NULL && core_cfg->router_enabled);

    if (is_router) {
        /* Promiscuous filter for router to inspect all extended CAN-FD frames */
        struct can_filter filter = {
            .flags = CAN_FILTER_IDE,
            .id = 0,
            .mask = 0
        };
        rx_filter_ids[0] = can_add_rx_filter(m_can_dev, can_rx_callback, NULL, &filter);
        if (rx_filter_ids[0] < 0) {
            LOG_ERR("Failed to add router CAN RX filter: %d", rx_filter_ids[0]);
            return rx_filter_ids[0];
        }
    } else {
        /* Filter for local node ID */
        deos_node_id_t local_node = core_cfg ? core_cfg->node_id : DEOS_NODE_INVALID;
        int filter_idx = 0;

        if (local_node != DEOS_NODE_INVALID) {
            struct can_filter local_filter = {
                .flags = CAN_FILTER_IDE,
                .id = ((uint32_t)local_node << DEOS_CAN_DESTINATION_SHIFT),
                .mask = ((uint32_t)DEOS_CAN_DESTINATION_MASK << DEOS_CAN_DESTINATION_SHIFT)
            };
            rx_filter_ids[filter_idx] = can_add_rx_filter(m_can_dev, can_rx_callback, NULL, &local_filter);
            if (rx_filter_ids[filter_idx] < 0) {
                LOG_ERR("Failed to add local CAN RX filter: %d", rx_filter_ids[filter_idx]);
                return rx_filter_ids[filter_idx];
            }
            filter_idx++;
        }

        /* Filter for Global Broadcast */
        struct can_filter broadcast_filter = {
            .flags = CAN_FILTER_IDE,
            .id = ((uint32_t)DEOS_NODE_GLOBAL_BROADCAST << DEOS_CAN_DESTINATION_SHIFT),
            .mask = ((uint32_t)DEOS_CAN_DESTINATION_MASK << DEOS_CAN_DESTINATION_SHIFT)
        };
        rx_filter_ids[filter_idx] = can_add_rx_filter(m_can_dev, can_rx_callback, NULL, &broadcast_filter);
        if (rx_filter_ids[filter_idx] < 0) {
            LOG_ERR("Failed to add global broadcast CAN RX filter: %d", rx_filter_ids[filter_idx]);
            return rx_filter_ids[filter_idx];
        }
        filter_idx++;

        /* Filter for CAN Broadcast */
        struct can_filter can_broadcast_filter = {
            .flags = CAN_FILTER_IDE,
            .id = ((uint32_t)DEOS_NODE_CAN_BROADCAST << DEOS_CAN_DESTINATION_SHIFT),
            .mask = ((uint32_t)DEOS_CAN_DESTINATION_MASK << DEOS_CAN_DESTINATION_SHIFT)
        };
        rx_filter_ids[filter_idx] = can_add_rx_filter(m_can_dev, can_rx_callback, NULL, &can_broadcast_filter);
        if (rx_filter_ids[filter_idx] < 0) {
            LOG_ERR("Failed to add CAN broadcast filter: %d", rx_filter_ids[filter_idx]);
            return rx_filter_ids[filter_idx];
        }
        filter_idx++;

        /* For non-router endpoint node, set CAN-FD as default transport */
        deos_set_default_transport(DEOS_TRANSPORT_CAN_FD);
    }

    /* Register with DEOS core router */
    deos_router_register_transport(DEOS_TRANSPORT_CAN_FD, deos_can_transport_tx);

    m_is_initialized = true;
    LOG_INF("DEOS CAN-FD Transport initialized (%s)", m_can_dev->name);

    return 0;
}

int deos_can_transport_start(void)
{
    if (!m_is_initialized || !m_can_dev) {
        return -ENODEV;
    }

    if (m_is_started) {
        return -EALREADY;
    }

    int ret = can_start(m_can_dev);
    if (ret != 0 && ret != -EALREADY) {
        LOG_ERR("Failed to start CAN device %s: %d", m_can_dev->name, ret);
        return ret;
    }

    k_thread_create(
        &can_rx_thread_data,
        can_rx_thread_stack,
        K_KERNEL_STACK_SIZEOF(can_rx_thread_stack),
        can_rx_thread_func,
        NULL, NULL, NULL,
        DEOS_CAN_THREAD_PRIORITY,
        0, K_NO_WAIT);

    k_thread_create(
        &can_tx_thread_data,
        can_tx_thread_stack,
        K_KERNEL_STACK_SIZEOF(can_tx_thread_stack),
        can_tx_thread_func,
        NULL, NULL, NULL,
        DEOS_CAN_THREAD_PRIORITY,
        0, K_NO_WAIT);

    m_is_started = true;
    LOG_INF("DEOS CAN-FD Transport started");

    return 0;
}

bool deos_can_transport_is_ready(void)
{
    return m_is_initialized && m_is_started;
}

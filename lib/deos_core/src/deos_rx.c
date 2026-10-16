#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <errno.h>

#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(deos_rx, LOG_LEVEL_INF);

static atomic_t rx_overflow_flag = ATOMIC_INIT(0);

/*
 * Static RX Queue
 */


K_MSGQ_DEFINE(rx_msgq, sizeof(struct can_frame), DEOS_RX_QUEUE_SIZE, 4);

/*
 * RX Worker Thread
 */


static struct k_thread rx_thread_data;
static K_KERNEL_STACK_DEFINE(rx_thread_stack, DEOS_RX_THREAD_STACK_SIZE);
#define DEOS_MAX_RX_FILTERS (DEOS_MAX_LOCAL_NODES + 1)
static int rx_filter_ids[DEOS_MAX_RX_FILTERS];

/*
 * CAN RX Callback (Runs in interrupt context)
 */
static void deos_can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data)
{
    /* Push to static queue, do not block */
    if (k_msgq_put(&rx_msgq, frame, K_NO_WAIT) != 0) {
        atomic_set(&rx_overflow_flag, 1);
    }
}

/*
 * DEOS RX Thread Loop
 */
static void deos_rx_thread_func(void *p1, void *p2, void *p3)
{
    struct can_frame frame = {0};
    deos_message_t msg;

    LOG_INF("DEOS RX Thread started");

    while (1) {
        if (atomic_cas(&rx_overflow_flag, 1, 0)) {
            deos_fault_raise(DEOS_FAULT_RX_QUEUE_OVERFLOW, DEOS_FAULT_SEVERITY_ERROR);
        }

        if (k_msgq_get(&rx_msgq, &frame, K_FOREVER) == 0) {
            int ret = deos_decode_frame(&frame, &msg);
            
            if (ret != 0) {
                LOG_WRN("Failed to decode frame: %d", ret);
                deos_fault_raise(DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
                continue;
            }

            /* Valid message, pass to dispatcher */
            deos_dispatch(&msg, DEOS_TRANSPORT_CAN_FD);
        }
    }
}

int deos_rx_init(void)
{
    const struct deos_config *config = deos_get_config();
    if (!config || !config->can_dev) {
        return 0; /* No CAN device configured: graceful no-op */
    }

    for (int i = 0; i < DEOS_MAX_RX_FILTERS; i++) {
        rx_filter_ids[i] = -1;
    }

    if (!device_is_ready(config->can_dev)) {
        LOG_ERR("CAN device not ready");
        return -ENODEV;
    }

    /* 
     * CAN controller mode configuration.
     * We depend on the app to configure bitrate in devicetree or beforehand.
     * However, we must ensure CAN-FD mode is active if the driver requires it.
     */
    int ret = can_set_mode(config->can_dev, CAN_MODE_FD);
    if (ret != 0 && ret != -ENOTSUP) {
        /* 
         * Note: Some hardware (like nucleo_f439zi) might not support CAN-FD.
         * The instruction says "Board CAN-FD support etmiyorsa hata dönmesi kabul edilebilir. 
         * Classic CAN'e fallback YAPMA."
         * If the driver doesn't support CAN_MODE_FD, it might return -ENOTSUP.
         * We fail outright if it doesn't support CAN-FD.
         */
        LOG_ERR("Failed to set CAN-FD mode: %d", ret);
        return ret;
    }
    
    if (ret == -ENOTSUP) {
        LOG_ERR("CAN-FD mode not supported by hardware!");
        return ret;
    }

    LOG_DBG("RX infrastructure initialized");
    return 0;
}

int deos_rx_start(void)
{
    const struct deos_config *config = deos_get_config();
    if (!config || !config->can_dev) {
        return 0; /* No CAN device configured: graceful no-op */
    }

    if (config->router_enabled) {
        /* Add CAN filter for router (promiscuous for extended DEOS frames) */
        struct can_filter filter = {
            .flags = CAN_FILTER_IDE, /* Only care about extended frames */
            .id = 0,
            .mask = 0 /* Promiscuous mode */
        };

        rx_filter_ids[0] = can_add_rx_filter(config->can_dev, deos_can_rx_callback, NULL, &filter);
        if (rx_filter_ids[0] < 0) {
            LOG_ERR("Failed to add router RX filter: %d", rx_filter_ids[0]);
            return rx_filter_ids[0];
        }
    } else {
        /* Filter for all registered local nodes */
        deos_node_id_t local_nodes[DEOS_MAX_LOCAL_NODES];
        int num_nodes = deos_get_local_nodes(local_nodes, DEOS_MAX_LOCAL_NODES);
        
        int filter_idx = 0;
        for (int i = 0; i < num_nodes; i++) {
            struct can_filter local_filter = {
                .flags = CAN_FILTER_IDE,
                .id = ((uint32_t)local_nodes[i] << DEOS_CAN_DESTINATION_SHIFT),
                .mask = ((uint32_t)DEOS_CAN_DESTINATION_MASK << DEOS_CAN_DESTINATION_SHIFT)
            };
            
            rx_filter_ids[filter_idx] = can_add_rx_filter(config->can_dev, deos_can_rx_callback, NULL, &local_filter);
            if (rx_filter_ids[filter_idx] < 0) {
                LOG_ERR("Failed to add local RX filter for node 0x%02X: %d", local_nodes[i], rx_filter_ids[filter_idx]);
                for (int j = 0; j < filter_idx; j++) {
                    can_remove_rx_filter(config->can_dev, rx_filter_ids[j]);
                    rx_filter_ids[j] = -1;
                }
                return rx_filter_ids[filter_idx];
            }
            filter_idx++;
        }

        /* Filter for Broadcast destination */
        struct can_filter broadcast_filter = {
            .flags = CAN_FILTER_IDE,
            .id = ((uint32_t)DEOS_NODE_GLOBAL_BROADCAST << DEOS_CAN_DESTINATION_SHIFT),
            .mask = ((uint32_t)DEOS_CAN_DESTINATION_MASK << DEOS_CAN_DESTINATION_SHIFT)
        };
        
        rx_filter_ids[filter_idx] = can_add_rx_filter(config->can_dev, deos_can_rx_callback, NULL, &broadcast_filter);
        if (rx_filter_ids[filter_idx] < 0) {
            LOG_ERR("Failed to add global broadcast RX filter: %d", rx_filter_ids[filter_idx]);
            for (int j = 0; j < filter_idx; j++) {
                can_remove_rx_filter(config->can_dev, rx_filter_ids[j]);
                rx_filter_ids[j] = -1;
            }
            return rx_filter_ids[filter_idx];
        }
        filter_idx++;

        /* Filter for CAN Broadcast destination */
        struct can_filter can_broadcast_filter = {
            .flags = CAN_FILTER_IDE,
            .id = ((uint32_t)DEOS_NODE_CAN_BROADCAST << DEOS_CAN_DESTINATION_SHIFT),
            .mask = ((uint32_t)DEOS_CAN_DESTINATION_MASK << DEOS_CAN_DESTINATION_SHIFT)
        };
        
        rx_filter_ids[filter_idx] = can_add_rx_filter(config->can_dev, deos_can_rx_callback, NULL, &can_broadcast_filter);
        if (rx_filter_ids[filter_idx] < 0) {
            LOG_ERR("Failed to add CAN broadcast RX filter: %d", rx_filter_ids[filter_idx]);
            for (int j = 0; j < filter_idx; j++) {
                can_remove_rx_filter(config->can_dev, rx_filter_ids[j]);
                rx_filter_ids[j] = -1;
            }
            return rx_filter_ids[filter_idx];
        }
    }
    
    /* Start CAN device */
    int ret = can_start(config->can_dev);
    if (ret != 0 && ret != -EALREADY) {
        LOG_ERR("Failed to start CAN device: %d", ret);
        return ret;
    }

    /* Create static thread */
    k_thread_create(
        &rx_thread_data,
        rx_thread_stack,
        K_KERNEL_STACK_SIZEOF(rx_thread_stack),
        deos_rx_thread_func,
        NULL, NULL, NULL,
        DEOS_RX_THREAD_PRIORITY,
        0, K_NO_WAIT);

    return 0;
}

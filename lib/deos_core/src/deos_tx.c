#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>
#include <errno.h>

#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(deos_tx, LOG_LEVEL_INF);

K_MSGQ_DEFINE(tx_msgq, sizeof(deos_message_t), DEOS_RX_QUEUE_SIZE, 4);
static struct k_thread tx_thread_data;
static K_KERNEL_STACK_DEFINE(tx_thread_stack, DEOS_RX_THREAD_STACK_SIZE);

static void deos_tx_thread_func(void *p1, void *p2, void *p3)
{
    deos_message_t msg;
    LOG_INF("DEOS CAN TX Thread started");

    while (1) {
        if (k_msgq_get(&tx_msgq, &msg, K_FOREVER) == 0) {
            const struct deos_config *config = deos_get_config();
            if (config && config->can_dev) {
                struct can_frame frame = {0};
                if (deos_encode_frame(&msg, &frame) == 0) {
                    /* Blocking call here won't block the caller threads */
                    int ret = can_send(config->can_dev, &frame, K_MSEC(100), NULL, NULL);
                    if (ret != 0) {
                        LOG_ERR("CAN send failed: %d", ret);
                        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
                    } else {
                        LOG_DBG("Sent MSG (Cmd: 0x%02X) Seq: %u", msg.command, msg.sequence);
                    }
                }
            }
        }
    }
}

int deos_tx_init(void)
{
    LOG_DBG("TX infrastructure initialized");
    return 0;
}

int deos_tx_start(void)
{
    k_thread_create(
        &tx_thread_data,
        tx_thread_stack,
        K_KERNEL_STACK_SIZEOF(tx_thread_stack),
        deos_tx_thread_func,
        NULL, NULL, NULL,
        DEOS_RX_THREAD_PRIORITY,
        0, K_NO_WAIT);
    return 0;
}

int deos_internal_canfd_tx(const deos_message_t *msg)
{
    const struct deos_config *config = deos_get_config();
    if (!config || !config->can_dev) {
        return -ENODEV;
    }

    if (k_msgq_put(&tx_msgq, msg, K_NO_WAIT) != 0) {
        LOG_WRN("TX Queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }
    return 0;
}


int deos_send_from_node(
    deos_node_id_t source_node,
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len)
{
    const struct deos_config *config = deos_get_config();
    if (!config) {
        return -ENODEV;
    }

    if (!deos_is_local_node(source_node)) {
        return -EPERM; /* Prevent spoofing */
    }

    if (payload_len > DEOS_MAX_PAYLOAD_LEN) {
        return -EMSGSIZE;
    }

    if (payload_len > 0 && payload == NULL) {
        return -EINVAL;
    }

    deos_message_t msg = {
        .priority      = priority,
        .message_class = message_class,
        .service       = service,
        .destination   = destination,
        .source        = source_node,
        .version       = DEOS_PROTOCOL_VERSION,
        .sequence      = deos_next_sequence_for_node(source_node),
        .command       = command,
        .payload_len   = payload_len
    };

    if (payload_len > 0) {
        memcpy(msg.payload, payload, payload_len);
    }

    if (config->router_enabled) {
        deos_dispatch(&msg, DEOS_TRANSPORT_LOCAL);
        return 0;
    }

    /* Direct CAN send for non-router endpoint nodes */
    if (!config->can_dev) {
        LOG_WRN("Cannot send: router disabled and CAN device not configured");
        return -ENODEV;
    }

    /* Fallback directly to queue if router is disabled */
    if (k_msgq_put(&tx_msgq, &msg, K_NO_WAIT) != 0) {
        LOG_WRN("TX Queue full, dropping message");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return -ENOSPC;
    }

    return 0;
}

int deos_send(
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len)
{
    const struct deos_config *config = deos_get_config();
    if (!config) {
        return -ENODEV;
    }

    return deos_send_from_node(
        config->node_id,
        destination,
        priority,
        message_class,
        service,
        command,
        payload,
        payload_len
    );
}

int deos_send_response(
    const deos_message_t *original,
    deos_result_t result,
    const void *data,
    uint8_t data_len)
{
    const struct deos_config *config = deos_get_config();
    if (!config) {
        return -ENODEV;
    }

    if (!original) {
        return -EINVAL;
    }

    /* Maximum optional data is DEOS_MAX_PAYLOAD_LEN - 1 (for result byte) */
    if (data_len > (DEOS_MAX_PAYLOAD_LEN - 1)) {
        return -EMSGSIZE;
    }

    if (data_len > 0 && data == NULL) {
        return -EINVAL;
    }

    deos_node_id_t source = original->destination;
    if (source == DEOS_NODE_GLOBAL_BROADCAST || source == DEOS_NODE_CAN_BROADCAST) {
        source = config->node_id;
    }

    uint8_t payload[DEOS_MAX_PAYLOAD_LEN];
    payload[0] = (uint8_t)result;

    if (data_len > 0) {
        memcpy(&payload[1], data, data_len);
    }

    return deos_send_from_node(
        source,
        original->source,
        original->priority,
        DEOS_CLASS_RESPONSE,
        original->service,
        original->command,
        payload,
        data_len + 1
    );
}

#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(deos_send, LOG_LEVEL_INF);

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
        .payload_len   = (uint8_t)payload_len
    };

    if (payload_len > 0) {
        memcpy(msg.payload, payload, payload_len);
    }

    /* All outbound messages enter core dispatch via LOCAL */
    deos_dispatch(&msg, DEOS_TRANSPORT_LOCAL);
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

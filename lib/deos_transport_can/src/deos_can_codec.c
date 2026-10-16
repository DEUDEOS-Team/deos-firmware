#include <deos/deos_can.h>
#include <string.h>
#include <errno.h>

int deos_can_id_encode(
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    deos_node_id_t destination,
    deos_node_id_t source,
    uint32_t *can_id)
{
    if (!can_id) {
        return -EINVAL;
    }

    /* Validations based on mask width */
    if (priority > DEOS_CAN_PRIORITY_MASK ||
        message_class > DEOS_CAN_MESSAGE_CLASS_MASK ||
        service > DEOS_CAN_SERVICE_MASK) {
        return -EINVAL;
    }

    *can_id = ((uint32_t)priority << DEOS_CAN_PRIORITY_SHIFT) |
              ((uint32_t)message_class << DEOS_CAN_MESSAGE_CLASS_SHIFT) |
              ((uint32_t)service << DEOS_CAN_SERVICE_SHIFT) |
              ((uint32_t)destination << DEOS_CAN_DESTINATION_SHIFT) |
              ((uint32_t)source << DEOS_CAN_SOURCE_SHIFT);

    return 0;
}

int deos_can_id_decode(
    uint32_t can_id,
    deos_message_t *message)
{
    if (!message) {
        return -EINVAL;
    }

    message->priority      = (deos_priority_t)((can_id >> DEOS_CAN_PRIORITY_SHIFT) & DEOS_CAN_PRIORITY_MASK);
    message->message_class = (deos_message_class_t)((can_id >> DEOS_CAN_MESSAGE_CLASS_SHIFT) & DEOS_CAN_MESSAGE_CLASS_MASK);
    message->service       = (deos_service_id_t)((can_id >> DEOS_CAN_SERVICE_SHIFT) & DEOS_CAN_SERVICE_MASK);
    message->destination   = (deos_node_id_t)((can_id >> DEOS_CAN_DESTINATION_SHIFT) & DEOS_CAN_DESTINATION_MASK);
    message->source        = (deos_node_id_t)((can_id >> DEOS_CAN_SOURCE_SHIFT) & DEOS_CAN_SOURCE_MASK);

    return 0;
}

int deos_can_encode_frame(
    const deos_message_t *msg,
    struct can_frame *frame)
{
    if (!msg || !frame) {
        return -EINVAL;
    }

    if (msg->payload_len > DEOS_MAX_PAYLOAD_LEN) {
        return -EMSGSIZE;
    }

    int ret = deos_can_id_encode(
        msg->priority,
        msg->message_class,
        msg->service,
        msg->destination,
        msg->source,
        &frame->id);

    if (ret != 0) {
        return ret;
    }

    /* Set as Extended, CAN-FD and Bit Rate Switch (BRS) */
    frame->flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS;

    /* Frame length = header size + payload_len */
    uint8_t semantic_len = DEOS_COMMON_HEADER_SIZE + msg->payload_len;
    frame->dlc = can_bytes_to_dlc(semantic_len);

    frame->data[DEOS_VERSION_OFFSET]  = msg->version;
    frame->data[DEOS_SEQUENCE_OFFSET] = msg->sequence;
    frame->data[DEOS_COMMAND_OFFSET]  = msg->command;
    frame->data[DEOS_LENGTH_OFFSET]   = (uint8_t)msg->payload_len;

    if (msg->payload_len > 0) {
        memcpy(&frame->data[DEOS_PAYLOAD_OFFSET], msg->payload, msg->payload_len);
    }

    /* Zero out the rest of the CAN-FD frame payload to avoid dirty bytes if it rounds up DLC */
    uint8_t physical_len = can_dlc_to_bytes(frame->dlc);
    if (physical_len > semantic_len) {
        memset(&frame->data[semantic_len], 0, physical_len - semantic_len);
    }

    return 0;
}

int deos_can_decode_frame(
    const struct can_frame *frame,
    deos_message_t *msg)
{
    if (!frame || !msg) {
        return -EINVAL;
    }

    if ((frame->flags & CAN_FRAME_IDE) == 0) {
        return -EPROTO; /* Only extended ID supported */
    }

    if ((frame->flags & CAN_FRAME_FDF) == 0) {
        return -EPROTO; /* Only CAN-FD supported */
    }

    /* Determine actual received physical length */
    uint8_t physical_len = can_dlc_to_bytes(frame->dlc);
    
    if (physical_len < DEOS_COMMON_HEADER_SIZE) {
        return -EMSGSIZE; /* Too small for header */
    }

    int ret = deos_can_id_decode(frame->id, msg);
    if (ret != 0) {
        return ret;
    }

    if (msg->source == DEOS_NODE_INVALID || msg->destination == DEOS_NODE_INVALID) {
        return -EPROTO;
    }

    msg->version     = frame->data[DEOS_VERSION_OFFSET];
    msg->sequence    = frame->data[DEOS_SEQUENCE_OFFSET];
    msg->command     = frame->data[DEOS_COMMAND_OFFSET];
    msg->payload_len = frame->data[DEOS_LENGTH_OFFSET];

    if (msg->payload_len > DEOS_MAX_PAYLOAD_LEN) {
        return -EMSGSIZE;
    }

    if (DEOS_COMMON_HEADER_SIZE + msg->payload_len > physical_len) {
        return -EMSGSIZE;
    }

    /* Validate protocol version - Exact match required for 4-byte header format */
    if (msg->version != DEOS_PROTOCOL_VERSION) {
        return -EPROTONOSUPPORT;
    }

    if (msg->payload_len > 0) {
        memcpy(msg->payload, &frame->data[DEOS_PAYLOAD_OFFSET], msg->payload_len);
    }

    return 0;
}

#include <deos/deos_frame.h>
#include <string.h>
#include <errno.h>

int deos_frame_encode(const deos_message_t *msg, uint8_t *buf, size_t buf_max)
{
    if (!msg || !buf) {
        return -EINVAL;
    }
    if ((size_t)(DEOS_UNIVERSAL_HEADER_SIZE + msg->payload_len) > buf_max) {
        return -ENOSPC;
    }

    buf[DEOS_UF_DEST_OFFSET]    = (uint8_t)msg->destination;
    buf[DEOS_UF_SRC_OFFSET]     = (uint8_t)msg->source;
    buf[DEOS_UF_PRIO_OFFSET]    = (uint8_t)msg->priority;
    buf[DEOS_UF_VER_OFFSET]     = (msg->version != 0) ? msg->version : DEOS_PROTOCOL_VERSION;
    buf[DEOS_UF_CLASS_OFFSET]   = (uint8_t)msg->message_class;
    buf[DEOS_UF_SERVICE_OFFSET] = (uint8_t)msg->service;
    buf[DEOS_UF_CMD_OFFSET]     = (uint8_t)msg->command;
    buf[DEOS_UF_SEQ_OFFSET]     = (uint8_t)msg->sequence;
    buf[DEOS_UF_LEN_OFFSET]     = (uint8_t)msg->payload_len;

    if (msg->payload_len > 0 && msg->payload_len <= DEOS_MAX_PAYLOAD_LEN) {
        memcpy(&buf[DEOS_UF_PAYLOAD_OFFSET], msg->payload, msg->payload_len);
    }

    return DEOS_UNIVERSAL_HEADER_SIZE + msg->payload_len;
}

int deos_frame_decode(const uint8_t *buf, size_t frame_len, deos_message_t *msg)
{
    if (!buf || !msg) {
        return -EINVAL;
    }
    if (frame_len < DEOS_UNIVERSAL_HEADER_SIZE) {
        return -EINVAL;
    }

    uint8_t payload_len = buf[DEOS_UF_LEN_OFFSET];
    if ((size_t)(DEOS_UNIVERSAL_HEADER_SIZE + payload_len) > frame_len) {
        return -EBADMSG;
    }
    if (payload_len > DEOS_MAX_PAYLOAD_LEN) {
        return -EMSGSIZE;
    }

    memset(msg, 0, sizeof(*msg));
    msg->destination   = (deos_node_id_t)buf[DEOS_UF_DEST_OFFSET];
    msg->source        = (deos_node_id_t)buf[DEOS_UF_SRC_OFFSET];
    msg->priority      = (deos_priority_t)buf[DEOS_UF_PRIO_OFFSET];
    msg->version       = buf[DEOS_UF_VER_OFFSET];
    msg->message_class = (deos_message_class_t)buf[DEOS_UF_CLASS_OFFSET];
    msg->service       = (deos_service_id_t)buf[DEOS_UF_SERVICE_OFFSET];
    msg->command       = buf[DEOS_UF_CMD_OFFSET];
    msg->sequence      = buf[DEOS_UF_SEQ_OFFSET];
    msg->payload_len   = payload_len;

    if (payload_len > 0) {
        memcpy(msg->payload, &buf[DEOS_UF_PAYLOAD_OFFSET], payload_len);
    }

    return 0;
}

const char *deos_node_name(uint8_t node_id)
{
    switch (node_id) {
        case DEOS_NODE_INVALID:          return "INVALID";
        case DEOS_NODE_TEXTUAL:          return "TEXTUAL";
        case DEOS_NODE_GROUND_CONTROL:   return "GROUND_CONTROL";
        case DEOS_NODE_MAIN_STM32:       return "MAIN_STM32";
        case DEOS_NODE_ROS_BRIDGE:       return "ROS_BRIDGE";
        case DEOS_NODE_TRACTION:         return "TRACTION";
        case DEOS_NODE_STEERING:         return "STEERING";
        case DEOS_NODE_BRAKE:            return "BRAKE";
        case DEOS_NODE_IMU:              return "IMU";
        case DEOS_NODE_BMS_MAIN:         return "BMS_MAIN";
        case DEOS_NODE_BMS_AUX:          return "BMS_AUX";
        case DEOS_NODE_BMS_GATEWAY:      return "BMS_GATEWAY";
        case DEOS_NODE_DIAG_TOOL:        return "DIAG_TOOL";
        case DEOS_NODE_CAN_BROADCAST:    return "CAN_BROADCAST";
        case DEOS_NODE_GLOBAL_BROADCAST: return "GLOBAL_BROADCAST";
        default:                         return "UNKNOWN_NODE";
    }
}

const char *deos_transport_name(deos_transport_t transport)
{
    switch (transport) {
        case DEOS_TRANSPORT_LOCAL:          return "LOCAL";
        case DEOS_TRANSPORT_CAN_FD:         return "CAN_FD";
        case DEOS_TRANSPORT_ETH_TEXTUAL:    return "ETH_TEXTUAL";
        case DEOS_TRANSPORT_ETH_ROS_BRIDGE: return "ETH_ROS_BRIDGE";
        case DEOS_TRANSPORT_UART_LORA:      return "UART_LORA";
        default:                            return "UNKNOWN_TRP";
    }
}

const char *deos_service_name(uint8_t service)
{
    switch (service) {
        case DEOS_SERVICE_SYSTEM:      return "SYSTEM";
        case DEOS_SERVICE_TRACTION:    return "TRACTION";
        case DEOS_SERVICE_STEERING:    return "STEERING";
        case DEOS_SERVICE_BRAKE:       return "BRAKE";
        case DEOS_SERVICE_BMS:         return "BMS";
        case DEOS_SERVICE_CALIBRATION: return "CALIBRATION";
        case DEOS_SERVICE_DIAGNOSTIC:  return "DIAGNOSTIC";
        case DEOS_SERVICE_SENSORS:     return "SENSORS";
        case DEOS_SERVICE_LIGHTING:    return "LIGHTING";
        default:                       return "UNKNOWN_SVC";
    }
}

const char *deos_class_name(uint8_t msg_class)
{
    switch (msg_class) {
        case DEOS_CLASS_SAFETY:   return "SAFETY";
        case DEOS_CLASS_COMMAND:  return "COMMAND";
        case DEOS_CLASS_STATUS:   return "STATUS";
        case DEOS_CLASS_EVENT:    return "EVENT";
        case DEOS_CLASS_REQUEST:  return "REQUEST";
        case DEOS_CLASS_RESPONSE: return "RESPONSE";
        case DEOS_CLASS_CONFIG:   return "CONFIG";
        case DEOS_CLASS_NETWORK:  return "NETWORK";
        default:                  return "UNKNOWN_CLS";
    }
}

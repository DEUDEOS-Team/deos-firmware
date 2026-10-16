#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(deos_network, LOG_LEVEL_INF);

int deos_send_ping(
    deos_node_id_t destination,
    uint32_t ping_id)
{
    if (destination == DEOS_NODE_GLOBAL_BROADCAST || destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_ERR("Broadcast PING rejected");
        return -EINVAL; /* Do not allow broadcast ping to avoid response storm */
    }

    uint8_t payload[4];
    deos_put_u32_le(payload, ping_id);

    return deos_send(
        destination,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_NETWORK,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_PING,
        payload,
        sizeof(payload));
}

int deos_send_heartbeat_from_node(deos_node_id_t source_node, deos_state_t current_state)
{
    if (!deos_is_local_node(source_node)) {
        LOG_ERR("Cannot send heartbeat from unregistered node 0x%02X", source_node);
        return -EPERM;
    }

    struct deos_heartbeat_payload payload;
    payload.current_state = current_state;

    return deos_send_from_node(
        source_node,
        DEOS_NODE_BROADCAST,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_NETWORK,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_HEARTBEAT,
        &payload,
        sizeof(payload));
}

int deos_send_heartbeat(deos_state_t current_state)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_send_heartbeat_from_node(config->node_id, current_state);
}

void deos_handle_ping(const deos_message_t *msg)
{
    if (msg->destination == DEOS_NODE_GLOBAL_BROADCAST || msg->destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_WRN("Broadcast PING from 0x%02X ignored", msg->source);
        return;
    }

    if (msg->payload_len != 4) {
        LOG_WRN("Invalid PING payload length: %d", msg->payload_len);
        return;
    }

    uint32_t ping_id = deos_get_u32_le(msg->payload);
    LOG_DBG("Received PING from 0x%02X with ID 0x%08X", msg->source, ping_id);

    /* Automatically generate PONG */
    /* PONG sequence will use normal node sequence via deos_send_from_node */
    int ret = deos_send_from_node(
        msg->destination, /* Reply from the targeted local node */
        msg->source,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_NETWORK,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_PONG,
        msg->payload, /* Reflect Ping ID */
        msg->payload_len);

    if (ret != 0) {
        LOG_ERR("Failed to send PONG to 0x%02X: %d", msg->source, ret);
    }
}

void deos_handle_pong(const deos_message_t *msg)
{
    if (msg->payload_len != 4) {
        LOG_WRN("Invalid PONG payload length: %d", msg->payload_len);
        return;
    }

    uint32_t ping_id = deos_get_u32_le(msg->payload);
    LOG_DBG("Received PONG from 0x%02X for ID 0x%08X", msg->source, ping_id);
    
    /* Transaction correlation is provisional and can be added here in the future. */
}

/*
 * Node Info (GET_NODE_INFO) Storage and Handlers
 */
struct deos_local_node_info {
    bool configured;
    uint8_t hw_type;
    uint8_t fw_version_major;
    uint8_t fw_version_minor;
    uint8_t fw_version_patch;
    uint32_t git_hash;
    uint32_t supported_services;
};

static struct deos_local_node_info local_node_info_tbl[DEOS_MAX_LOCAL_NODES];

int deos_set_node_info_for_node(deos_node_id_t node_id, uint8_t hw_type,
                                uint8_t fw_maj, uint8_t fw_min, uint8_t fw_patch,
                                uint32_t git_hash, uint32_t supported_services)
{
    int idx = deos_get_local_node_index(node_id);
    if (idx < 0) {
        LOG_ERR("Cannot configure node info for unregistered node 0x%02X", node_id);
        return -EPERM;
    }

    local_node_info_tbl[idx].configured         = true;
    local_node_info_tbl[idx].hw_type            = hw_type;
    local_node_info_tbl[idx].fw_version_major   = fw_maj;
    local_node_info_tbl[idx].fw_version_minor   = fw_min;
    local_node_info_tbl[idx].fw_version_patch   = fw_patch;
    local_node_info_tbl[idx].git_hash           = git_hash;
    local_node_info_tbl[idx].supported_services = supported_services;

    LOG_INF("Node info configured for 0x%02X (HW: 0x%02X, FW: %u.%u.%u)",
            node_id, hw_type, fw_maj, fw_min, fw_patch);
    return 0;
}

int deos_set_node_info(uint8_t hw_type, uint8_t fw_maj, uint8_t fw_min, uint8_t fw_patch,
                       uint32_t git_hash, uint32_t supported_services)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_set_node_info_for_node(config->node_id, hw_type, fw_maj, fw_min, fw_patch,
                                       git_hash, supported_services);
}

int deos_send_node_info_request_from_node(deos_node_id_t source_node, deos_node_id_t destination)
{
    if (destination == DEOS_NODE_GLOBAL_BROADCAST || destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_ERR("Broadcast GET_NODE_INFO request rejected");
        return -EINVAL;
    }

    if (!deos_is_local_node(source_node)) {
        LOG_ERR("Cannot send GET_NODE_INFO request from unregistered node 0x%02X", source_node);
        return -EPERM;
    }

    return deos_send_from_node(
        source_node,
        destination,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_REQUEST,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_GET_NODE_INFO,
        NULL,
        0);
}

int deos_send_node_info_request(deos_node_id_t destination)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_send_node_info_request_from_node(config->node_id, destination);
}

void deos_handle_get_node_info(const deos_message_t *msg)
{
    if (!msg) {
        return;
    }

    if (msg->destination == DEOS_NODE_GLOBAL_BROADCAST || msg->destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_WRN("Broadcast GET_NODE_INFO from 0x%02X ignored", msg->source);
        return;
    }

    int idx = deos_get_local_node_index(msg->destination);
    if (idx < 0) {
        LOG_WRN("GET_NODE_INFO for non-local node 0x%02X", msg->destination);
        return;
    }

    struct deos_node_info_payload resp;
    memset(&resp, 0, sizeof(resp));

    resp.result_code   = (uint8_t)DEOS_RESULT_SUCCESS;
    resp.node_id       = (uint8_t)msg->destination;
    resp.proto_version = DEOS_PROTOCOL_VERSION; /* 0x11 = v1.1 */
    resp.reserved      = 0x00;

    if (local_node_info_tbl[idx].configured) {
        resp.hw_type            = local_node_info_tbl[idx].hw_type;
        resp.fw_version_major   = local_node_info_tbl[idx].fw_version_major;
        resp.fw_version_minor   = local_node_info_tbl[idx].fw_version_minor;
        resp.fw_version_patch   = local_node_info_tbl[idx].fw_version_patch;
        resp.git_hash           = local_node_info_tbl[idx].git_hash;
        resp.supported_services = local_node_info_tbl[idx].supported_services;
    } else {
        /* Defaults if not explicitly configured by app */
#if defined(CONFIG_SOC_SERIES_STM32H5X)
        resp.hw_type = DEOS_HW_TYPE_STM32H5;
#elif defined(CONFIG_SOC_SERIES_STM32F4X)
        resp.hw_type = DEOS_HW_TYPE_STM32F4;
#else
        resp.hw_type = DEOS_HW_TYPE_UNKNOWN;
#endif
        resp.fw_version_major   = 1;
        resp.fw_version_minor   = 0;
        resp.fw_version_patch   = 0;
        resp.git_hash           = 0;
        resp.supported_services = (1U << DEOS_SERVICE_SYSTEM);
    }

    uint32_t uptime_s = (uint32_t)(k_uptime_get() / 1000);
    resp.uptime_s = uptime_s;

    LOG_DBG("Sending NODE_INFO response to 0x%02X from 0x%02X (Uptime: %u s)",
            msg->source, msg->destination, uptime_s);

    int ret = deos_send_from_node(
        msg->destination,
        msg->source,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_RESPONSE,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_GET_NODE_INFO,
        &resp,
        sizeof(resp));

    if (ret != 0) {
        LOG_ERR("Failed to send NODE_INFO response to 0x%02X: %d", msg->source, ret);
    }
}

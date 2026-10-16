#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(deos_core, LOG_LEVEL_INF);

static struct deos_config core_config;
static bool is_initialized = false;

struct deos_local_node_context {
    deos_node_id_t node_id;
    atomic_t tx_sequence;
    bool in_use;
};

static struct deos_local_node_context local_nodes[DEOS_MAX_LOCAL_NODES];
static bool is_started = false;

extern int deos_rx_init(void);
extern int deos_rx_start(void);

int deos_init(const struct deos_config *config)
{
    if (is_initialized) {
        return -EALREADY;
    }

    if (!config) {
        return -EINVAL;
    }

    if (config->node_id == DEOS_NODE_INVALID || config->node_id == DEOS_NODE_GLOBAL_BROADCAST || config->node_id == DEOS_NODE_CAN_BROADCAST) {
        LOG_ERR("Invalid local node ID: 0x%02X", config->node_id);
        return -EINVAL;
    }

    /* Save configuration */
    core_config = *config;

    for (int i = 0; i < DEOS_MAX_LOCAL_NODES; i++) {
        local_nodes[i].in_use = false;
    }
    local_nodes[0].node_id = core_config.node_id;
    local_nodes[0].in_use = true;
    atomic_set(&local_nodes[0].tx_sequence, 0);

    int ret = deos_fault_init();
    if (ret != 0) {
        LOG_ERR("Failed to init fault manager");
        return ret;
    }

    ret = deos_dispatch_init();
    if (ret != 0) {
        LOG_ERR("Failed to init dispatcher");
        return ret;
    }

    if (core_config.router_enabled) {
        ret = deos_router_init();
        if (ret != 0) {
            LOG_ERR("Failed to init router");
            return ret;
        }
        if (core_config.can_dev != NULL) {
            deos_router_register_transport(DEOS_TRANSPORT_CAN_FD, deos_internal_canfd_tx);
        }
    }

    if (core_config.can_dev != NULL) {
        ret = deos_rx_init();
        if (ret != 0) {
            LOG_ERR("Failed to init RX infrastructure");
            return ret;
        }

        ret = deos_tx_init();
        if (ret != 0) {
            LOG_ERR("Failed to init TX infrastructure");
            return ret;
        }
    } else {
        LOG_INF("CAN device not configured; CAN-FD transport disabled");
    }

    ret = deos_subscription_init();
    if (ret != 0) {
        LOG_ERR("Failed to init subscription engine");
        return ret;
    }

    is_initialized = true;
    LOG_INF("DEOS Core initialized (Node ID: 0x%02X)", core_config.node_id);

    return 0;
}

int deos_start(void)
{
    if (!is_initialized) {
        return -EPERM;
    }

    if (is_started) {
        return -EALREADY;
    }

    if (core_config.can_dev != NULL) {
        int ret = deos_rx_start();
        if (ret != 0) {
            LOG_ERR("Failed to start RX threads");
            return ret;
        }

        ret = deos_tx_start();
        if (ret != 0) {
            LOG_ERR("Failed to start TX threads");
            return ret;
        }
    }

    int ret = deos_subscription_start();
    if (ret != 0) {
        LOG_ERR("Failed to start subscription scheduler");
        return ret;
    }

    is_started = true;
    LOG_INF("DEOS Core started");
    return 0;
}

bool deos_can_is_available(void)
{
    return is_initialized && (core_config.can_dev != NULL);
}

const struct deos_config *deos_get_config(void)
{
    return &core_config;
}

int deos_register_local_node(deos_node_id_t node_id)
{
    if (!is_initialized) return -EPERM;
    if (is_started) return -EBUSY;
    if (!core_config.hosted_nodes_enabled) return -ENOTSUP;
    if (node_id == DEOS_NODE_INVALID || node_id == DEOS_NODE_GLOBAL_BROADCAST || node_id == DEOS_NODE_CAN_BROADCAST) return -EINVAL;

    /* Check duplicate */
    for (int i = 0; i < DEOS_MAX_LOCAL_NODES; i++) {
        if (local_nodes[i].in_use && local_nodes[i].node_id == node_id) {
            return -EALREADY;
        }
    }

    /* Find empty slot */
    for (int i = 0; i < DEOS_MAX_LOCAL_NODES; i++) {
        if (!local_nodes[i].in_use) {
            local_nodes[i].node_id = node_id;
            local_nodes[i].in_use = true;
            atomic_set(&local_nodes[i].tx_sequence, 0);
            return 0;
        }
    }

    return -ENOSPC;
}

bool deos_is_local_node(deos_node_id_t node_id)
{
    return deos_get_local_node_index(node_id) >= 0;
}

int deos_get_local_node_index(deos_node_id_t node_id)
{
    if (node_id == DEOS_NODE_INVALID || node_id == DEOS_NODE_GLOBAL_BROADCAST || node_id == DEOS_NODE_CAN_BROADCAST) {
        return -1;
    }
    for (int i = 0; i < DEOS_MAX_LOCAL_NODES; i++) {
        if (local_nodes[i].in_use && local_nodes[i].node_id == node_id) {
            return i;
        }
    }
    return -1;
}

int deos_get_local_nodes(deos_node_id_t *nodes, int max_nodes)
{
    int count = 0;
    for (int i = 0; i < DEOS_MAX_LOCAL_NODES && count < max_nodes; i++) {
        if (local_nodes[i].in_use) {
            nodes[count++] = local_nodes[i].node_id;
        }
    }
    return count;
}

uint8_t deos_next_sequence_for_node(deos_node_id_t node_id)
{
    for (int i = 0; i < DEOS_MAX_LOCAL_NODES; i++) {
        if (local_nodes[i].in_use && local_nodes[i].node_id == node_id) {
            atomic_val_t seq = atomic_inc(&local_nodes[i].tx_sequence);
            return (uint8_t)(seq & 0xFF);
        }
    }
    return 0;
}

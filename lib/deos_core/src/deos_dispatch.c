#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(deos_dispatch, LOG_LEVEL_INF);



struct deos_handler_entry {
    bool in_use;
    deos_node_id_t local_node;
    deos_message_class_t message_class;
    deos_service_id_t service;
    uint8_t command;
    deos_message_handler_t handler;
    void *user_data;
};

static struct deos_handler_entry handler_registry[DEOS_MAX_HANDLERS];

int deos_dispatch_init(void)
{
    memset(handler_registry, 0, sizeof(handler_registry));
    LOG_DBG("Dispatcher initialized");
    return 0;
}

int deos_register_handler_for_node(
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    if (!handler) {
        return -EINVAL;
    }

    if (!deos_is_local_node(local_node)) {
        return -EPERM;
    }

    int free_slot = -1;

    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (!handler_registry[i].in_use) {
            if (free_slot == -1) {
                free_slot = i;
            }
        } else {
            if (handler_registry[i].local_node == local_node &&
                handler_registry[i].message_class == message_class &&
                handler_registry[i].service == service &&
                handler_registry[i].command == command) {
                return -EEXIST; /* Duplicate */
            }
        }
    }

    if (free_slot == -1) {
        return -ENOSPC; /* Full */
    }

    handler_registry[free_slot].local_node    = local_node;
    handler_registry[free_slot].message_class = message_class;
    handler_registry[free_slot].service       = service;
    handler_registry[free_slot].command       = command;
    handler_registry[free_slot].handler       = handler;
    handler_registry[free_slot].user_data     = user_data;
    handler_registry[free_slot].in_use        = true;

    LOG_DBG("Registered handler for Node: 0x%02X Class: 0x%X, Svc: 0x%X, Cmd: 0x%X",
            local_node, message_class, service, command);

    return 0;
}

int deos_register_handler(
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_register_handler_for_node(
        config->node_id, message_class, service, command, handler, user_data);
}

deos_message_handler_t deos_dispatch_find_handler(
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    void **out_user_data)
{
    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (handler_registry[i].in_use &&
            handler_registry[i].local_node == local_node &&
            handler_registry[i].message_class == message_class &&
            handler_registry[i].service == service &&
            handler_registry[i].command == command) {
            if (out_user_data) {
                *out_user_data = handler_registry[i].user_data;
            }
            return handler_registry[i].handler;
        }
    }
    return NULL;
}

void deos_dispatch(const deos_message_t *msg, deos_transport_t incoming_transport)
{
    const struct deos_config *config = deos_get_config();
    bool is_local = deos_is_local_node(msg->destination) || (msg->destination == DEOS_NODE_GLOBAL_BROADCAST) || (msg->destination == DEOS_NODE_CAN_BROADCAST);

    /* Router Logic */
    if (config->router_enabled && (!deos_is_local_node(msg->destination) || msg->destination == DEOS_NODE_GLOBAL_BROADCAST || msg->destination == DEOS_NODE_CAN_BROADCAST)) {
        deos_route_message(msg, incoming_transport);
    }

    if (!is_local) {
        return;
    }

    /* Common Network Behaviour */
    if (msg->message_class == DEOS_CLASS_NETWORK && msg->service == DEOS_SERVICE_SYSTEM) {
        switch (msg->command) {
            case DEOS_CMD_SYSTEM_PING:
                deos_handle_ping(msg);
                return;
            case DEOS_CMD_SYSTEM_PONG:
                deos_handle_pong(msg);
                return;
            case DEOS_CMD_SYSTEM_HEARTBEAT:
                /* Do not auto-consume heartbeat, let application handle it */
                break;
            default:
                break; /* Allow app to handle other system network cmds */
        }
    }

    /* Common Subscription Behaviour */
    if (msg->service == DEOS_SERVICE_SYSTEM && msg->message_class == DEOS_CLASS_REQUEST) {
        if (msg->command == DEOS_CMD_SYSTEM_SUBSCRIBE) {
            deos_subscription_handle_subscribe(msg);
            return;
        } else if (msg->command == DEOS_CMD_SYSTEM_UNSUBSCRIBE) {
            deos_subscription_handle_unsubscribe(msg);
            return;
        }
    }

    /* Common Diagnostic Behaviour */
    if (msg->service == DEOS_SERVICE_DIAGNOSTIC) {
        if (msg->message_class == DEOS_CLASS_REQUEST && msg->command == DEOS_CMD_DIAG_GET_FAULTS) {
            deos_fault_handle_get_faults(msg);
            return;
        } else if (msg->message_class == DEOS_CLASS_COMMAND && msg->command == DEOS_CMD_DIAG_CLEAR_FAULTS) {
            deos_fault_handle_clear_faults(msg);
            return;
        }
    }

    /* Application Handlers */
    bool handled = false;
    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (handler_registry[i].in_use &&
            (handler_registry[i].local_node == msg->destination || msg->destination == DEOS_NODE_GLOBAL_BROADCAST || msg->destination == DEOS_NODE_CAN_BROADCAST) &&
            handler_registry[i].message_class == msg->message_class &&
            handler_registry[i].service == msg->service &&
            handler_registry[i].command == msg->command) {
            
            int ret = handler_registry[i].handler(msg, handler_registry[i].user_data);
            if (ret != 0) {
                LOG_WRN("Handler returned error %d for Cmd: 0x%X", ret, msg->command);
            }
            handled = true;
            /* TODO(architecture): If this is a BROADCAST message, should we break here 
             * or continue dispatching to ALL other local nodes that registered this handler? 
             * Current DEOS semantics deliver it once to the first matching handler. */
            break; /* Assume one handler per command */
        }
    }

    if (!handled) {
        LOG_DBG("No handler for Class: 0x%X, Svc: 0x%X, Cmd: 0x%X",
                msg->message_class, msg->service, msg->command);
    }
}

void deos_feed_message(const deos_message_t *msg, deos_transport_t transport)
{
    if (msg) {
        deos_dispatch(msg, transport);
    }
}

int deos_feed_frame(const uint8_t *frame_data, size_t frame_len, deos_transport_t transport)
{
    if (!frame_data) {
        return -EINVAL;
    }

    deos_message_t msg;
    int ret = deos_frame_decode(frame_data, frame_len, &msg);
    if (ret != 0) {
        LOG_WRN("DEOS RX Error: Corrupt frame (%d) via %s", ret, deos_transport_name(transport));
        deos_fault_raise(DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
        return ret;
    }

    deos_dispatch(&msg, transport);
    return 0;
}

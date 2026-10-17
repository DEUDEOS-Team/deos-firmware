/**
 * @file deos_registry.c
 * @brief Pure ANSI C implementation of DEOS Message Handler Registry
 */

#include <deos/deos_registry.h>
#include <string.h>
#include <errno.h>

int deos_registry_init(deos_registry_t *reg)
{
    if (!reg) {
        return -EINVAL;
    }

    memset(reg, 0, sizeof(*reg));
    return 0;
}

int deos_registry_register_for_node(
    deos_registry_t *reg,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    if (!reg || !handler) {
        return -EINVAL;
    }

    int free_slot = -1;

    for (int i = 0; i < DEOS_MAX_REGISTRY_HANDLERS; i++) {
        if (!reg->entries[i].in_use) {
            if (free_slot == -1) {
                free_slot = i;
            }
        } else {
            if (reg->entries[i].local_node == local_node &&
                reg->entries[i].message_class == message_class &&
                reg->entries[i].service == service &&
                reg->entries[i].command == command) {
                return -EEXIST; /* Duplicate registration */
            }
        }
    }

    if (free_slot == -1) {
        return -ENOSPC; /* Table full */
    }

    reg->entries[free_slot].in_use        = true;
    reg->entries[free_slot].local_node    = local_node;
    reg->entries[free_slot].message_class = message_class;
    reg->entries[free_slot].service       = service;
    reg->entries[free_slot].command       = command;
    reg->entries[free_slot].handler       = handler;
    reg->entries[free_slot].user_data     = user_data;
    reg->count++;

    return 0;
}

int deos_registry_register(
    deos_registry_t *reg,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    /* 0xFF acts as wildcard matching any incoming local destination */
    return deos_registry_register_for_node(
        reg, (deos_node_id_t)0xFF, message_class, service, command, handler, user_data);
}

int deos_registry_unregister(
    deos_registry_t *reg,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command)
{
    if (!reg) {
        return -EINVAL;
    }

    for (int i = 0; i < DEOS_MAX_REGISTRY_HANDLERS; i++) {
        if (reg->entries[i].in_use &&
            reg->entries[i].local_node == local_node &&
            reg->entries[i].message_class == message_class &&
            reg->entries[i].service == service &&
            reg->entries[i].command == command) {

            memset(&reg->entries[i], 0, sizeof(reg->entries[i]));
            if (reg->count > 0) {
                reg->count--;
            }
            return 0;
        }
    }

    return -ENOENT;
}

deos_message_handler_t deos_registry_find(
    const deos_registry_t *reg,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    void **out_user_data)
{
    if (!reg) {
        return NULL;
    }

    for (int i = 0; i < DEOS_MAX_REGISTRY_HANDLERS; i++) {
        if (reg->entries[i].in_use &&
            (reg->entries[i].local_node == local_node || reg->entries[i].local_node == 0xFF) &&
            reg->entries[i].message_class == message_class &&
            reg->entries[i].service == service &&
            reg->entries[i].command == command) {

            if (out_user_data) {
                *out_user_data = reg->entries[i].user_data;
            }
            return reg->entries[i].handler;
        }
    }

    return NULL;
}

int deos_registry_dispatch(
    const deos_registry_t *reg,
    const deos_message_t *msg)
{
    if (!reg || !msg) {
        return -EINVAL;
    }

    int matched_count = 0;
    bool is_broadcast = (msg->destination == DEOS_NODE_GLOBAL_BROADCAST ||
                         msg->destination == DEOS_NODE_CAN_BROADCAST);

    for (int i = 0; i < DEOS_MAX_REGISTRY_HANDLERS; i++) {
        if (!reg->entries[i].in_use) {
            continue;
        }

        /* Match node destination */
        bool node_matches = (reg->entries[i].local_node == msg->destination) ||
                            (reg->entries[i].local_node == 0xFF) ||
                            is_broadcast;

        if (node_matches &&
            reg->entries[i].message_class == msg->message_class &&
            reg->entries[i].service == msg->service &&
            reg->entries[i].command == msg->command) {

            reg->entries[i].handler(msg, reg->entries[i].user_data);
            matched_count++;

            /* Unicast messages stop on first exact node match */
            if (!is_broadcast && reg->entries[i].local_node != 0xFF) {
                break;
            }
        }
    }

    return matched_count;
}

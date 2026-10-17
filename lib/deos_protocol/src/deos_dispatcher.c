/**
 * @file deos_dispatcher.c
 * @brief Pure ANSI C implementation of DEOS Message Dispatcher
 */

#include <deos/deos_dispatcher.h>
#include <string.h>
#include <errno.h>

int deos_dispatcher_init(deos_dispatcher_t *disp)
{
    if (!disp) {
        return -EINVAL;
    }

    memset(disp, 0, sizeof(*disp));
    return 0;
}

int deos_dispatcher_register_for_node(
    deos_dispatcher_t *disp,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    if (!disp || !handler) {
        return -EINVAL;
    }

    int free_slot = -1;

    for (int i = 0; i < DEOS_MAX_DISPATCHER_HANDLERS; i++) {
        if (!disp->entries[i].in_use) {
            if (free_slot == -1) {
                free_slot = i;
            }
        } else {
            if (disp->entries[i].local_node == local_node &&
                disp->entries[i].message_class == message_class &&
                disp->entries[i].service == service &&
                disp->entries[i].command == command) {
                return -EEXIST; /* Duplicate registration */
            }
        }
    }

    if (free_slot == -1) {
        return -ENOSPC; /* Table full */
    }

    disp->entries[free_slot].in_use        = true;
    disp->entries[free_slot].local_node    = local_node;
    disp->entries[free_slot].message_class = message_class;
    disp->entries[free_slot].service       = service;
    disp->entries[free_slot].command       = command;
    disp->entries[free_slot].handler       = handler;
    disp->entries[free_slot].user_data     = user_data;
    disp->count++;

    return 0;
}

int deos_dispatcher_register(
    deos_dispatcher_t *disp,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    /* 0xFF acts as wildcard matching any incoming local destination */
    return deos_dispatcher_register_for_node(
        disp, (deos_node_id_t)0xFF, message_class, service, command, handler, user_data);
}

int deos_dispatcher_unregister(
    deos_dispatcher_t *disp,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command)
{
    if (!disp) {
        return -EINVAL;
    }

    for (int i = 0; i < DEOS_MAX_DISPATCHER_HANDLERS; i++) {
        if (disp->entries[i].in_use &&
            disp->entries[i].local_node == local_node &&
            disp->entries[i].message_class == message_class &&
            disp->entries[i].service == service &&
            disp->entries[i].command == command) {

            memset(&disp->entries[i], 0, sizeof(disp->entries[i]));
            if (disp->count > 0) {
                disp->count--;
            }
            return 0;
        }
    }

    return -ENOENT;
}

deos_message_handler_t deos_dispatcher_find(
    const deos_dispatcher_t *disp,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    void **out_user_data)
{
    if (!disp) {
        return NULL;
    }

    for (int i = 0; i < DEOS_MAX_DISPATCHER_HANDLERS; i++) {
        if (disp->entries[i].in_use &&
            (disp->entries[i].local_node == local_node || disp->entries[i].local_node == 0xFF) &&
            disp->entries[i].message_class == message_class &&
            disp->entries[i].service == service &&
            disp->entries[i].command == command) {

            if (out_user_data) {
                *out_user_data = disp->entries[i].user_data;
            }
            return disp->entries[i].handler;
        }
    }

    return NULL;
}

int deos_dispatcher_dispatch(
    const deos_dispatcher_t *disp,
    const deos_message_t *msg)
{
    if (!disp || !msg) {
        return -EINVAL;
    }

    int matched_count = 0;
    bool is_broadcast = (msg->destination == DEOS_NODE_GLOBAL_BROADCAST ||
                         msg->destination == DEOS_NODE_CAN_BROADCAST);

    for (int i = 0; i < DEOS_MAX_DISPATCHER_HANDLERS; i++) {
        if (!disp->entries[i].in_use) {
            continue;
        }

        /* Match node destination */
        bool node_matches = (disp->entries[i].local_node == msg->destination) ||
                            (disp->entries[i].local_node == 0xFF) ||
                            is_broadcast;

        if (node_matches &&
            disp->entries[i].message_class == msg->message_class &&
            disp->entries[i].service == msg->service &&
            disp->entries[i].command == msg->command) {

            disp->entries[i].handler(msg, disp->entries[i].user_data);
            matched_count++;

            /* Unicast messages stop on first exact node match */
            if (!is_broadcast && disp->entries[i].local_node != 0xFF) {
                break;
            }
        }
    }

    return matched_count;
}

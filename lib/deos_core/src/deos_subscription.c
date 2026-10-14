#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(deos_sub, LOG_LEVEL_INF);

struct deos_subscription_entry {
    bool in_use;
    deos_node_id_t local_node;
    deos_node_id_t subscriber_node;
    deos_service_id_t service;
    uint8_t command;
    uint16_t interval_ms;
    int64_t last_sent_ms;
    int64_t expire_time_ms;
};

static struct deos_subscription_entry sub_table[DEOS_MAX_SUBSCRIPTIONS];
static struct k_mutex sub_lock;
static struct k_work_delayable sub_work;
static bool sub_started = false;

static void sub_work_handler(struct k_work *work)
{
    deos_subscription_process();
    if (sub_started) {
        k_work_reschedule(&sub_work, K_MSEC(10));
    }
}

int deos_subscription_init(void)
{
    k_mutex_init(&sub_lock);
    memset(sub_table, 0, sizeof(sub_table));
    k_work_init_delayable(&sub_work, sub_work_handler);
    LOG_DBG("Subscription engine initialized (Max slots: %d)", DEOS_MAX_SUBSCRIPTIONS);
    return 0;
}

int deos_subscription_start(void)
{
    sub_started = true;
    k_work_reschedule(&sub_work, K_MSEC(10));
    LOG_DBG("Subscription engine scheduler started");
    return 0;
}

void deos_subscription_handle_subscribe(const deos_message_t *msg)
{
    if (!msg) return;

    if (msg->destination == DEOS_NODE_GLOBAL_BROADCAST || msg->destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_WRN("Broadcast subscribe from 0x%02X rejected", msg->source);
        deos_fault_raise(DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
        return;
    }

    if (msg->payload_len < sizeof(struct deos_subscribe_req_payload)) {
        LOG_ERR("Invalid SUBSCRIBE payload length: %d", msg->payload_len);
        deos_fault_raise_for_node(msg->destination, DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
        deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        return;
    }

    const struct deos_subscribe_req_payload *req =
        (const struct deos_subscribe_req_payload *)msg->payload;

    deos_node_id_t local_node = msg->destination;
    deos_node_id_t subscriber = msg->source;
    uint16_t interval_ms = deos_get_u16_le((const uint8_t *)&req->interval_ms);
    uint16_t lease_ttl_s = deos_get_u16_le((const uint8_t *)&req->lease_ttl_s);

    /* interval_ms == 0 means unsubscribe */
    if (interval_ms == 0) {
        k_mutex_lock(&sub_lock, K_FOREVER);
        for (int i = 0; i < DEOS_MAX_SUBSCRIPTIONS; i++) {
            if (sub_table[i].in_use &&
                sub_table[i].local_node == local_node &&
                sub_table[i].subscriber_node == subscriber &&
                sub_table[i].service == req->target_service &&
                sub_table[i].command == req->target_command) {
                sub_table[i].in_use = false;
                LOG_INF("Unsubscribed 0x%02X from Svc: 0x%02X, Cmd: 0x%02X",
                        subscriber, req->target_service, req->target_command);
            }
        }
        k_mutex_unlock(&sub_lock);
        deos_send_response(msg, DEOS_RESULT_SUCCESS, NULL, 0);
        return;
    }

    /* Check if local node has an application handler registered for this (service, command) */
    void *user_data = NULL;
    deos_message_handler_t handler = deos_dispatch_find_handler(
        local_node, DEOS_CLASS_REQUEST, req->target_service, req->target_command, &user_data);

    if (!handler) {
        LOG_WRN("Subscription rejected: Node 0x%02X has no handler for Svc: 0x%02X, Cmd: 0x%02X",
                local_node, req->target_service, req->target_command);
        struct deos_subscribe_resp_payload resp = {0};
        resp.result_code = DEOS_RESULT_UNSUPPORTED;
        resp.target_service = req->target_service;
        resp.target_command = req->target_command;
        deos_put_u16_le((uint8_t *)&resp.actual_interval_ms, 0);
        deos_send_response(msg, DEOS_RESULT_UNSUPPORTED, &resp, sizeof(resp));
        return;
    }

    /* Enforce minimum resolution of 10 ms */
    uint16_t actual_interval = (interval_ms < 10) ? 10 : interval_ms;

    k_mutex_lock(&sub_lock, K_FOREVER);

    int existing_slot = -1;
    int free_slot = -1;

    for (int i = 0; i < DEOS_MAX_SUBSCRIPTIONS; i++) {
        if (sub_table[i].in_use) {
            if (sub_table[i].local_node == local_node &&
                sub_table[i].subscriber_node == subscriber &&
                sub_table[i].service == req->target_service &&
                sub_table[i].command == req->target_command) {
                existing_slot = i;
                break;
            }
        } else if (free_slot == -1) {
            free_slot = i;
        }
    }

    int target_slot = (existing_slot != -1) ? existing_slot : free_slot;

    if (target_slot == -1) {
        k_mutex_unlock(&sub_lock);
        LOG_WRN("Subscription table full on node 0x%02X", local_node);
        deos_fault_raise_for_node(local_node, DEOS_FAULT_RX_QUEUE_OVERFLOW, DEOS_FAULT_SEVERITY_WARNING);
        struct deos_subscribe_resp_payload resp = {0};
        resp.result_code = DEOS_RESULT_BUSY;
        resp.target_service = req->target_service;
        resp.target_command = req->target_command;
        deos_put_u16_le((uint8_t *)&resp.actual_interval_ms, 0);
        deos_send_response(msg, DEOS_RESULT_BUSY, &resp, sizeof(resp));
        return;
    }

    int64_t now = k_uptime_get();
    sub_table[target_slot].in_use = true;
    sub_table[target_slot].local_node = local_node;
    sub_table[target_slot].subscriber_node = subscriber;
    sub_table[target_slot].service = (deos_service_id_t)req->target_service;
    sub_table[target_slot].command = req->target_command;
    sub_table[target_slot].interval_ms = actual_interval;
    sub_table[target_slot].last_sent_ms = now;
    sub_table[target_slot].expire_time_ms = (lease_ttl_s > 0) ? (now + (int64_t)lease_ttl_s * 1000) : 0;

    k_mutex_unlock(&sub_lock);

    LOG_INF("Node 0x%02X subscribed 0x%02X to Svc: 0x%02X Cmd: 0x%02X @ %d ms (TTL: %d s)",
            local_node, subscriber, req->target_service, req->target_command, actual_interval, lease_ttl_s);

    struct deos_subscribe_resp_payload resp = {0};
    resp.result_code = DEOS_RESULT_SUCCESS;
    resp.target_service = req->target_service;
    resp.target_command = req->target_command;
    deos_put_u16_le((uint8_t *)&resp.actual_interval_ms, actual_interval);

    deos_send_response(msg, DEOS_RESULT_SUCCESS, &resp, sizeof(resp));
}

void deos_subscription_handle_unsubscribe(const deos_message_t *msg)
{
    if (!msg) return;

    if (msg->destination == DEOS_NODE_GLOBAL_BROADCAST || msg->destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_WRN("Broadcast unsubscribe from 0x%02X rejected", msg->source);
        deos_fault_raise(DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
        return;
    }

    if (msg->payload_len < sizeof(struct deos_unsubscribe_req_payload)) {
        LOG_ERR("Invalid UNSUBSCRIBE payload length: %d", msg->payload_len);
        deos_fault_raise_for_node(msg->destination, DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
        deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        return;
    }

    const struct deos_unsubscribe_req_payload *req =
        (const struct deos_unsubscribe_req_payload *)msg->payload;

    deos_node_id_t local_node = msg->destination;
    deos_node_id_t subscriber = msg->source;

    k_mutex_lock(&sub_lock, K_FOREVER);
    int removed_count = 0;

    for (int i = 0; i < DEOS_MAX_SUBSCRIPTIONS; i++) {
        if (sub_table[i].in_use &&
            sub_table[i].local_node == local_node &&
            sub_table[i].subscriber_node == subscriber) {
            
            bool match = false;
            if (req->target_service == 0xFF) {
                match = true; /* All services */
            } else if (sub_table[i].service == req->target_service) {
                if (req->target_command == 0xFF || sub_table[i].command == req->target_command) {
                    match = true;
                }
            }

            if (match) {
                sub_table[i].in_use = false;
                removed_count++;
            }
        }
    }
    k_mutex_unlock(&sub_lock);

    LOG_INF("Node 0x%02X removed %d subscriptions for subscriber 0x%02X",
            local_node, removed_count, subscriber);

    deos_send_response(msg, DEOS_RESULT_SUCCESS, NULL, 0);
}

void deos_subscription_process(void)
{
    int64_t now = k_uptime_get();

    for (int i = 0; i < DEOS_MAX_SUBSCRIPTIONS; i++) {
        struct deos_subscription_entry entry;
        bool should_trigger = false;

        k_mutex_lock(&sub_lock, K_FOREVER);
        if (sub_table[i].in_use) {
            /* Check TTL expiration */
            if (sub_table[i].expire_time_ms > 0 && now >= sub_table[i].expire_time_ms) {
                LOG_INF("Subscription expired: 0x%02X -> 0x%02X (Svc: 0x%02X Cmd: 0x%02X)",
                        sub_table[i].subscriber_node, sub_table[i].local_node,
                        sub_table[i].service, sub_table[i].command);
                sub_table[i].in_use = false;
            } else if (now - sub_table[i].last_sent_ms >= sub_table[i].interval_ms) {
                sub_table[i].last_sent_ms = now;
                entry = sub_table[i];
                should_trigger = true;
            }
        }
        k_mutex_unlock(&sub_lock);

        if (should_trigger) {
            /* Construct synthetic REQUEST message pretending to come from the subscriber */
            deos_message_t synth_msg;
            memset(&synth_msg, 0, sizeof(synth_msg));
            synth_msg.destination = entry.local_node;
            synth_msg.source = entry.subscriber_node;
            synth_msg.priority = DEOS_PRIO_STATUS;
            synth_msg.message_class = DEOS_CLASS_REQUEST;
            synth_msg.service = entry.service;
            synth_msg.command = entry.command;
            synth_msg.version = DEOS_PROTOCOL_VERSION;
            synth_msg.payload_len = 0;

            void *user_data = NULL;
            deos_message_handler_t handler = deos_dispatch_find_handler(
                entry.local_node, DEOS_CLASS_REQUEST, entry.service, entry.command, &user_data);

            if (handler) {
                int ret = handler(&synth_msg, user_data);
                if (ret != 0) {
                    LOG_DBG("Subscription handler returned %d for Svc: 0x%02X Cmd: 0x%02X",
                            ret, entry.service, entry.command);
                }
            } else {
                LOG_WRN("Registered subscription has no matching handler on node 0x%02X", entry.local_node);
                deos_fault_raise_for_node(entry.local_node, DEOS_FAULT_INTERNAL_SOFTWARE_ERROR, DEOS_FAULT_SEVERITY_ERROR);
            }
        }
    }
}

int deos_subscribe_from_node(deos_node_id_t source_node, deos_node_id_t destination,
                             deos_service_id_t service, uint8_t command,
                             uint16_t interval_ms, uint16_t lease_ttl_s)
{
    if (!deos_is_local_node(source_node)) {
        LOG_ERR("Cannot subscribe from non-local node 0x%02X", source_node);
        return -EPERM;
    }

    if (destination == DEOS_NODE_GLOBAL_BROADCAST || destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_ERR("Broadcast subscribe rejected");
        deos_fault_raise_for_node(source_node, DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
        return -EINVAL;
    }

    struct deos_subscribe_req_payload req;
    req.target_service = (uint8_t)service;
    req.target_command = command;
    deos_put_u16_le((uint8_t *)&req.interval_ms, interval_ms);
    deos_put_u16_le((uint8_t *)&req.lease_ttl_s, lease_ttl_s);

    return deos_send_from_node(
        source_node,
        destination,
        DEOS_PRIO_CONFIG,
        DEOS_CLASS_REQUEST,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_SUBSCRIBE,
        &req,
        sizeof(req));
}

int deos_subscribe(deos_node_id_t destination, deos_service_id_t service,
                   uint8_t command, uint16_t interval_ms, uint16_t lease_ttl_s)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_subscribe_from_node(
        config->node_id, destination, service, command, interval_ms, lease_ttl_s);
}

int deos_unsubscribe_from_node(deos_node_id_t source_node, deos_node_id_t destination,
                               deos_service_id_t service, uint8_t command)
{
    if (!deos_is_local_node(source_node)) {
        LOG_ERR("Cannot unsubscribe from non-local node 0x%02X", source_node);
        return -EPERM;
    }

    if (destination == DEOS_NODE_GLOBAL_BROADCAST || destination == DEOS_NODE_CAN_BROADCAST) {
        LOG_ERR("Broadcast unsubscribe rejected");
        deos_fault_raise_for_node(source_node, DEOS_FAULT_PROTOCOL_ERROR, DEOS_FAULT_SEVERITY_WARNING);
        return -EINVAL;
    }

    struct deos_unsubscribe_req_payload req;
    req.target_service = (uint8_t)service;
    req.target_command = command;

    return deos_send_from_node(
        source_node,
        destination,
        DEOS_PRIO_CONFIG,
        DEOS_CLASS_REQUEST,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_UNSUBSCRIBE,
        &req,
        sizeof(req));
}

int deos_unsubscribe(deos_node_id_t destination, deos_service_id_t service, uint8_t command)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_unsubscribe_from_node(
        config->node_id, destination, service, command);
}

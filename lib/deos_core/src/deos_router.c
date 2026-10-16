#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_router, LOG_LEVEL_INF);

/* Registry for transport TX callbacks */
static deos_transport_tx_fn_t transport_tx_registry[DEOS_TRANSPORT_COUNT];

/* Application-defined routing policy lookup */
static deos_router_lookup_fn_t app_route_lookup = NULL;

void deos_router_set_lookup_fn(deos_router_lookup_fn_t lookup_fn)
{
    app_route_lookup = lookup_fn;
    LOG_DBG("Routing policy lookup function set");
}

int deos_router_init(void)
{
    for (int i = 0; i < DEOS_TRANSPORT_COUNT; i++) {
        transport_tx_registry[i] = NULL;
    }
    LOG_INF("Router functionality initialized");
    return 0;
}

int deos_router_register_transport(deos_transport_t transport, deos_transport_tx_fn_t tx_fn)
{
    if (transport >= DEOS_TRANSPORT_COUNT) {
        return -EINVAL;
    }
    transport_tx_registry[transport] = tx_fn;
    LOG_DBG("Registered TX callback for transport %s (%d)", deos_transport_name(transport), transport);
    return 0;
}

deos_transport_tx_fn_t deos_router_get_transport_tx(deos_transport_t transport)
{
    if (transport < DEOS_TRANSPORT_COUNT) {
        return transport_tx_registry[transport];
    }
    return NULL;
}

static deos_transport_t default_transport = DEOS_TRANSPORT_UNKNOWN;

void deos_set_default_transport(deos_transport_t transport)
{
    default_transport = transport;
    LOG_DBG("Default transport set to %s (%d)", deos_transport_name(transport), transport);
}

deos_transport_t deos_get_default_transport(void)
{
    return default_transport;
}

void deos_route_message(const deos_message_t *msg, deos_transport_t incoming_transport)
{
    LOG_INF("Router RX: [%s (0x%02X) -> %s (0x%02X)] Cmd: 0x%02X, Len: %d (via %s)",
            deos_node_name(msg->source), msg->source,
            deos_node_name(msg->destination), msg->destination,
            msg->command, msg->payload_len,
            deos_transport_name(incoming_transport));

    if (!app_route_lookup) {
        LOG_WRN("Router DROP: No routing policy registered by application!");
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return;
    }

    deos_transport_t target_transport = app_route_lookup(msg->destination);

    if (target_transport == DEOS_TRANSPORT_LOCAL) {
        LOG_DBG("Router LOCAL: [%s (0x%02X) -> %s (0x%02X)] via LOCAL_IN",
                deos_node_name(msg->source), msg->source,
                deos_node_name(msg->destination), msg->destination);
        deos_dispatch_local(msg);
        return;
    }

    if (target_transport == DEOS_TRANSPORT_BROADCAST_ALL) {
        /* Run local handlers */
        deos_dispatch_local(msg);

        /* Route to all registered transports EXCEPT incoming */
        for (int i = 1; i < DEOS_TRANSPORT_COUNT; i++) {
            deos_transport_t t = (deos_transport_t)i;
            if (t != incoming_transport && transport_tx_registry[t] != NULL) {
                transport_tx_registry[t](msg);
            }
        }
        return;
    }

    /* Prevent Loopback and check if transport is registered */
    if (target_transport != DEOS_TRANSPORT_UNKNOWN && 
        target_transport != incoming_transport &&
        target_transport < DEOS_TRANSPORT_COUNT &&
        transport_tx_registry[target_transport] != NULL) {
        
        /* If destination is a CAN broadcast and local node is interested, also deliver locally */
        if (msg->destination == DEOS_NODE_CAN_BROADCAST && deos_is_local_node(msg->destination)) {
            deos_dispatch_local(msg);
        }

        LOG_DBG("Router FWD: [%s (0x%02X) -> %s (0x%02X)] via %s",
                deos_node_name(msg->source), msg->source,
                deos_node_name(msg->destination), msg->destination,
                deos_transport_name(target_transport));
        transport_tx_registry[target_transport](msg);
    } else {
        if (target_transport == incoming_transport) {
            LOG_DBG("Router: Skipping loopback for %s (0x%02X) via %s",
                    deos_node_name(msg->destination), msg->destination,
                    deos_transport_name(incoming_transport));
        } else {
            LOG_WRN("Router DROP: Mesaj iletilemedi! Hedef: %s (0x%02X) | Hat: %s (%d) (Gecersiz rota veya aktif TX yok)", 
                    deos_node_name(msg->destination), msg->destination,
                    deos_transport_name(target_transport), target_transport);
            deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        }
    }
}


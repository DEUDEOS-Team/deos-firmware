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

static const char *router_node_name(uint8_t id)
{
    switch (id) {
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
        default:                         return "UNKNOWN";
    }
}

static const char *router_transport_name(deos_transport_t trp)
{
    switch (trp) {
        case DEOS_TRANSPORT_LOCAL:          return "LOCAL";
        case DEOS_TRANSPORT_CAN_FD:         return "CAN_FD";
        case DEOS_TRANSPORT_ETH_TEXTUAL:    return "ETH_TEXTUAL";
        case DEOS_TRANSPORT_ETH_ROS_BRIDGE: return "ETH_ROS_BRIDGE";
        case DEOS_TRANSPORT_UART_LORA:      return "UART_LORA";
        default:                            return "UNKNOWN_TRP";
    }
}

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
    LOG_DBG("Registered TX callback for transport %s (%d)", router_transport_name(transport), transport);
    return 0;
}

void deos_route_message(const deos_message_t *msg, deos_transport_t incoming_transport)
{
    LOG_INF("Router RX: [%s (0x%02X) -> %s (0x%02X)] Cmd: 0x%02X, Len: %d (via %s)",
            router_node_name(msg->source), msg->source,
            router_node_name(msg->destination), msg->destination,
            msg->command, msg->payload_len,
            router_transport_name(incoming_transport));

    if (msg->destination == DEOS_NODE_GLOBAL_BROADCAST) {
        /* Route to all registered transports EXCEPT the one it came from (Loop Prevention) */
        for (int i = 1; i < DEOS_TRANSPORT_COUNT; i++) { /* Start at 1 to skip INTERNAL */
            deos_transport_t target_transport = (deos_transport_t)i;
            if (target_transport != incoming_transport && transport_tx_registry[target_transport] != NULL) {
                transport_tx_registry[target_transport](msg);
            }
        }
        return;
    }

    if (msg->destination == DEOS_NODE_CAN_BROADCAST) {
        /* Route ONLY to CAN_FD (if not incoming from it) */
        if (incoming_transport != DEOS_TRANSPORT_CAN_FD && transport_tx_registry[DEOS_TRANSPORT_CAN_FD] != NULL) {
            transport_tx_registry[DEOS_TRANSPORT_CAN_FD](msg);
        }
        return;
    }

    /* Unicast Routing Table (Application Policy) */
    deos_transport_t target_transport = DEOS_TRANSPORT_UNKNOWN;

    if (app_route_lookup) {
        target_transport = app_route_lookup(msg->destination);
    } else {
        /* Default fallback if application hasn't provided a routing policy */
        target_transport = DEOS_TRANSPORT_CAN_FD;
    }

    /* Prevent Loopback and check if transport is registered */
    if (target_transport != DEOS_TRANSPORT_UNKNOWN && 
        target_transport != incoming_transport &&
        transport_tx_registry[target_transport] != NULL) {
        
        LOG_DBG("Router FWD: [%s (0x%02X) -> %s (0x%02X)] via %s",
                router_node_name(msg->source), msg->source,
                router_node_name(msg->destination), msg->destination,
                router_transport_name(target_transport));
        transport_tx_registry[target_transport](msg);
    } else {
        if (target_transport == incoming_transport) {
            LOG_DBG("Router: Skipping loopback for %s (0x%02X) via %s",
                    router_node_name(msg->destination), msg->destination,
                    router_transport_name(incoming_transport));
        } else {
            LOG_WRN("Router DROP: Mesaj iletilemedi! Hedef: %s (0x%02X) | Hat: %s (%d) (Aktif TX callback yok)", 
                    router_node_name(msg->destination), msg->destination,
                    router_transport_name(target_transport), target_transport);
            deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        }
    }
}


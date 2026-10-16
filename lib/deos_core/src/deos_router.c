#include "deos_internal.h"
#include <deos/deos.h>
#include <deos/deos_fault.h>
#include <deos/deos_frame.h>
#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(deos_router, LOG_LEVEL_INF);

/* Registry for transport TX callbacks */
static deos_transport_tx_fn_t transport_tx_registry[DEOS_TRANSPORT_COUNT];

/* Application-defined routing policy lookup */
static deos_router_lookup_fn_t app_route_lookup = NULL;

void deos_router_set_lookup_fn(deos_router_lookup_fn_t lookup_fn) {
  app_route_lookup = lookup_fn;
  LOG_DBG("Routing policy lookup function set");
}

int deos_router_init(void) {
  for (int i = 0; i < DEOS_TRANSPORT_COUNT; i++) {
    transport_tx_registry[i] = NULL;
  }
  LOG_INF("Router functionality initialized");
  return 0;
}

int deos_router_register_transport(deos_transport_t transport,
                                   deos_transport_tx_fn_t tx_fn) {
  if (transport >= DEOS_TRANSPORT_COUNT) {
    return -EINVAL;
  }
  transport_tx_registry[transport] = tx_fn;
  LOG_DBG("Registered TX callback for transport %s (%d)",
          deos_transport_name(transport), transport);
  return 0;
}

void deos_route_message(const deos_message_t *msg,
                        deos_transport_t incoming_transport) {
  LOG_INF("[ROUTER >> GELEN] %s (0x%02X) -> %s (0x%02X) | Hat: %s | Srv: %s | "
          "Cmd: %s (0x%02X) | Cls: %s | Seq: %u | Boyut: %d B",
          deos_node_name(msg->source), msg->source,
          deos_node_name(msg->destination), msg->destination,
          deos_transport_name(incoming_transport),
          deos_service_name(msg->service),
          deos_command_name(msg->service, msg->command), msg->command,
          deos_class_name(msg->message_class), msg->sequence, msg->payload_len);

  if (!app_route_lookup) {
    LOG_WRN("[ROUTER !! DUSURULDU (DROP)] %s (0x%02X) -> %s (0x%02X) | Neden: "
            "Uygulama rota politikasi (c_routing_policy) tanimlamamis!",
            deos_node_name(msg->source), msg->source,
            deos_node_name(msg->destination), msg->destination);
    deos_fault_raise(DEOS_FAULT_ROUTER_NO_POLICY, DEOS_FAULT_SEVERITY_ERROR);
    return;
  }

  deos_transport_t target_transport = app_route_lookup(msg->destination);

  if (target_transport == DEOS_TRANSPORT_LOCAL) {
    LOG_INF("[ROUTER -> DUGUM ICI ISLENDI (LOCAL)] %s (0x%02X) -> %s (0x%02X) "
            "| Cmd: %s (0x%02X) | Yerel Handler'a teslim edildi",
            deos_node_name(msg->source), msg->source,
            deos_node_name(msg->destination), msg->destination,
            deos_command_name(msg->service, msg->command), msg->command);
    deos_dispatch_local(msg);
    return;
  }

  if (target_transport == DEOS_TRANSPORT_BROADCAST_ALL) {
    LOG_INF("[ROUTER -> GENEL YAYIN (BROADCAST)] %s (0x%02X) -> TUM DUGUMLER | "
            "Cmd: %s (0x%02X) | Gelen hat (%s) haric tum hatlara basiliyor",
            deos_node_name(msg->source), msg->source,
            deos_command_name(msg->service, msg->command), msg->command,
            deos_transport_name(incoming_transport));
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

    /* If destination is a CAN broadcast and local node is interested, also
     * deliver locally */
    if (msg->destination == DEOS_NODE_CAN_BROADCAST &&
        deos_is_local_node(msg->destination)) {
      deos_dispatch_local(msg);
    }

    LOG_INF("[ROUTER] %s (0x%02X) -> %s (0x%02X) | Hat: [%s -> %s] | Srv: %s | "
            "Cmd: %s (0x%02X)",
            deos_node_name(msg->source), msg->source,
            deos_node_name(msg->destination), msg->destination,
            deos_transport_name(incoming_transport),
            deos_transport_name(target_transport),
            deos_service_name(msg->service),
            deos_command_name(msg->service, msg->command), msg->command);
    transport_tx_registry[target_transport](msg);
  } else {
    if (target_transport == incoming_transport) {
      LOG_DBG(
          "[ROUTER(LOOPBACK)] src and dst: %s(0x%02X) | incoming transport: %s",
          deos_node_name(msg->destination), msg->destination,
          deos_transport_name(incoming_transport));
    } else {
      LOG_WRN("[ROUTER(DROP)] %s (0x%02X) -> %s (0x%02X) | Target Transport: "
              "%s (%d) | Cmd: %s (0x%02X)",
              deos_node_name(msg->source), msg->source,
              deos_node_name(msg->destination), msg->destination,
              deos_transport_name(target_transport), target_transport,
              deos_command_name(msg->service, msg->command), msg->command);
      if (target_transport == DEOS_TRANSPORT_UNKNOWN || target_transport >= DEOS_TRANSPORT_COUNT) {
          deos_fault_raise(DEOS_FAULT_ROUTER_UNMAPPED_ROUTE, DEOS_FAULT_SEVERITY_WARNING);
      } else {
          deos_fault_raise(DEOS_FAULT_ROUTER_TRP_UNAVAILABLE, DEOS_FAULT_SEVERITY_WARNING);
      }
    }
  }
}

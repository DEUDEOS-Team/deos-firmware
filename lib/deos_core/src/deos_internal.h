#ifndef DEOS_INTERNAL_H
#define DEOS_INTERNAL_H

#include <deos/deos_types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_DEOS_MAX_LOCAL_NODES
#define DEOS_MAX_LOCAL_NODES CONFIG_DEOS_MAX_LOCAL_NODES
#else
#define DEOS_MAX_LOCAL_NODES 4
#endif

#ifdef CONFIG_DEOS_MAX_FAULT_RECORDS
#define DEOS_MAX_FAULT_RECORDS CONFIG_DEOS_MAX_FAULT_RECORDS
#else
#define DEOS_MAX_FAULT_RECORDS 32
#endif

#ifdef CONFIG_DEOS_MAX_HANDLERS
#define DEOS_MAX_HANDLERS CONFIG_DEOS_MAX_HANDLERS
#else
#define DEOS_MAX_HANDLERS 32
#endif

#ifdef CONFIG_DEOS_MAX_SUBSCRIPTIONS
#define DEOS_MAX_SUBSCRIPTIONS CONFIG_DEOS_MAX_SUBSCRIPTIONS
#else
#define DEOS_MAX_SUBSCRIPTIONS 8
#endif

/*
 * Endian Helpers (Little-Endian is default for DEOS wire format)
 */
void deos_put_u16_le(uint8_t *dst, uint16_t value);
void deos_put_i16_le(uint8_t *dst, int16_t value);
void deos_put_u32_le(uint8_t *dst, uint32_t value);

uint16_t deos_get_u16_le(const uint8_t *src);
int16_t  deos_get_i16_le(const uint8_t *src);
uint32_t deos_get_u32_le(const uint8_t *src);

/*
 * Sequence and Node Management
 */
uint8_t deos_next_sequence_for_node(deos_node_id_t node_id);
bool deos_is_local_node(deos_node_id_t node_id);
int deos_get_local_node_index(deos_node_id_t node_id);
int deos_get_local_nodes(deos_node_id_t *nodes, int max_nodes);

/*
 * Dispatcher (deos_dispatch.c)
 */
int deos_dispatch_init(void);
void deos_dispatch(const deos_message_t *msg, deos_transport_t incoming_transport);
void deos_dispatch_local(const deos_message_t *msg);
deos_message_handler_t deos_dispatch_find_handler(
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    void **out_user_data);

/*
 * Subscription Engine (deos_subscription.c)
 */
int deos_subscription_init(void);
int deos_subscription_start(void);
void deos_subscription_handle_subscribe(const deos_message_t *msg);
void deos_subscription_handle_unsubscribe(const deos_message_t *msg);
void deos_subscription_process(void);

/*
 * Network / System (deos_network.c)
 */
void deos_handle_ping(const deos_message_t *msg);
void deos_handle_pong(const deos_message_t *msg);

/*
 * Router (deos_router.c)
 */
int deos_router_init(void);
void deos_route_message(const deos_message_t *msg, deos_transport_t incoming_transport);

/*
 * Fault Management (deos_fault.c)
 */
int deos_fault_init(void);
void deos_fault_handle_get_faults(const deos_message_t *msg);
void deos_fault_handle_clear_faults(const deos_message_t *msg);

/*
 * Core Internal Access
 */
const struct deos_config *deos_get_config(void);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_INTERNAL_H */

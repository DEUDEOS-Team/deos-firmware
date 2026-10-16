#ifndef DEOS_INTERNAL_H
#define DEOS_INTERNAL_H

#include <deos/deos_types.h>
#include <zephyr/drivers/can.h>
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

#ifdef CONFIG_DEOS_RX_QUEUE_SIZE
#define DEOS_RX_QUEUE_SIZE CONFIG_DEOS_RX_QUEUE_SIZE
#else
#define DEOS_RX_QUEUE_SIZE 16
#endif

#ifdef CONFIG_DEOS_RX_THREAD_STACK_SIZE
#define DEOS_RX_THREAD_STACK_SIZE CONFIG_DEOS_RX_THREAD_STACK_SIZE
#else
#define DEOS_RX_THREAD_STACK_SIZE 1024
#endif

#ifdef CONFIG_DEOS_RX_THREAD_PRIORITY
#define DEOS_RX_THREAD_PRIORITY CONFIG_DEOS_RX_THREAD_PRIORITY
#else
#define DEOS_RX_THREAD_PRIORITY 5
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
 * CAN ID and Frame Codec (deos_codec.c)
 */
int deos_can_id_encode(
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    deos_node_id_t destination,
    deos_node_id_t source,
    uint32_t *can_id);

int deos_can_id_decode(
    uint32_t can_id,
    deos_message_t *message);

int deos_encode_frame(
    const deos_message_t *msg,
    struct can_frame *frame);

int deos_decode_frame(
    const struct can_frame *frame,
    deos_message_t *msg);

int deos_internal_canfd_tx(const deos_message_t *msg);
int deos_tx_init(void);
int deos_tx_start(void);

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

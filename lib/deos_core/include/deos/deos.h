#ifndef DEOS_H
#define DEOS_H

#include <deos/deos_icd.h>
#include <deos/deos_types.h>
#include <deos/deos_frame.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the DEOS protocol stack.
 *
 * @param config The configuration structure for DEOS core.
 * @return 0 on success, negative error code on failure.
 */
int deos_init(const struct deos_config *config);

/**
 * @brief Start the DEOS protocol operations (e.g. start threads).
 *
 * @return 0 on success, negative error code on failure.
 */
int deos_start(void);

/**
 * @brief Check if CAN-FD transport is configured and operational.
 *
 * @return true if a CAN device is bound and initialized, false otherwise.
 */
bool deos_can_is_available(void);

/**
 * @brief Send a DEOS message.
 *
 * @param destination Target node ID.
 * @param priority Message priority.
 * @param message_class Message class.
 * @param service Service ID.
 * @param command Command ID.
 * @param payload Pointer to the command-specific payload.
 * @param payload_len Length of the payload (max 60 bytes).
 * @return 0 on success, negative error code on failure.
 */
int deos_send(deos_node_id_t destination, deos_priority_t priority,
              deos_message_class_t message_class, deos_service_id_t service,
              uint8_t command, const void *payload, size_t payload_len);

/**
 * @brief Send a PING message.
 *
 * @param destination Target node ID (Broadcast rejected).
 * @param ping_id 32-bit correlation ID.
 * @return 0 on success, negative error code on failure.
 */
int deos_send_ping(deos_node_id_t destination, uint32_t ping_id);

/**
 * @brief Register an application message handler.
 *
 * @param message_class Message class to listen for.
 * @param service Service ID to listen for.
 * @param command Command ID to listen for.
 * @param handler Callback function to invoke.
 * @param user_data User data passed to the callback.
 * @return 0 on success, negative error code on failure.
 */
int deos_register_handler(deos_message_class_t message_class,
                          deos_service_id_t service, uint8_t command,
                          deos_message_handler_t handler, void *user_data);

/**
 * @brief Register an additional local logical node.
 * Must be called before deos_start().
 */
int deos_register_local_node(deos_node_id_t node_id);

/**
 * @brief Register a handler for a specific local node.
 */
int deos_register_handler_for_node(deos_node_id_t local_node,
                                   deos_message_class_t message_class,
                                   deos_service_id_t service, uint8_t command,
                                   deos_message_handler_t handler,
                                   void *user_data);

/**
 * @brief Send a DEOS message from a specific local node.
 */
int deos_send_from_node(deos_node_id_t source_node, deos_node_id_t destination,
                        deos_priority_t priority,
                        deos_message_class_t message_class,
                        deos_service_id_t service, uint8_t command,
                        const void *payload, size_t payload_len);

/**
 * @brief Send a HEARTBEAT message.
 *
 * @return 0 on success, negative error code on failure.
 */
int deos_send_heartbeat(deos_state_t current_state);

/**
 * @brief Send a HEARTBEAT message from a specific local node.
 *
 * @param source_node The local node ID sending the heartbeat.
 * @return 0 on success, negative error code on failure.
 */
int deos_send_heartbeat_from_node(deos_node_id_t source_node,
                                  deos_state_t current_state);

/**
 * @brief Send a generic DEOS response.
 *
 * @param original Pointer to the original request/command message.
 * @param result The result code of the operation.
 * @param data Optional response data (up to 59 bytes).
 * @param data_len Length of the optional data.
 * @return 0 on success, negative error code on failure.
 */
int deos_send_response(const deos_message_t *original, deos_result_t result,
                       const void *data, uint8_t data_len);

/**
 * @brief Send a SUBSCRIBE request to a target node.
 *
 * @param destination Target producer node ID.
 * @param service Target service to subscribe to.
 * @param command Target status command to subscribe to.
 * @param interval_ms Desired streaming interval in milliseconds.
 * @param lease_ttl_s Lease duration in seconds (0 = infinite).
 * @return 0 on success, negative error code on failure.
 */
int deos_subscribe(deos_node_id_t destination, deos_service_id_t service,
                   uint8_t command, uint16_t interval_ms, uint16_t lease_ttl_s);

/**
 * @brief Send a SUBSCRIBE request from a specific local node.
 */
int deos_subscribe_from_node(deos_node_id_t source_node, deos_node_id_t destination,
                             deos_service_id_t service, uint8_t command,
                             uint16_t interval_ms, uint16_t lease_ttl_s);

/**
 * @brief Send an UNSUBSCRIBE request to a target node.
 *
 * @param destination Target producer node ID.
 * @param service Target service to unsubscribe from.
 * @param command Target command (0xFF = all commands).
 * @return 0 on success, negative error code on failure.
 */
int deos_unsubscribe(deos_node_id_t destination, deos_service_id_t service, uint8_t command);

/**
 * @brief Send an UNSUBSCRIBE request from a specific local node.
 */
int deos_unsubscribe_from_node(deos_node_id_t source_node, deos_node_id_t destination,
                               deos_service_id_t service, uint8_t command);

/**
 * @brief Notify ON_CHANGE subscribers of a state update for a specific local node.
 *
 * @param local_node The local node ID producing the state change.
 * @param service The service ID of the updated state.
 * @param command The command ID of the updated state.
 * @return Number of subscribers notified, or negative error code.
 */
int deos_subscription_notify_change_for_node(deos_node_id_t local_node,
                                             deos_service_id_t service,
                                             uint8_t command);

/**
 * @brief Notify ON_CHANGE subscribers of a state update (primary local node).
 */
int deos_subscription_notify_change(deos_service_id_t service, uint8_t command);

/**
 * @brief Transport TX function pointer type.
 */
typedef int (*deos_transport_tx_fn_t)(const deos_message_t *msg);

/**
 * @brief Register a transport interface for the router.
 *
 * @param transport The transport ID.
 * @param tx_fn The transmit function for this transport.
 * @return 0 on success.
 */
int deos_router_register_transport(deos_transport_t transport,
                                   deos_transport_tx_fn_t tx_fn);

/**
 * @brief Lookup function pointer type for dynamic routing.
 */
typedef deos_transport_t (*deos_router_lookup_fn_t)(deos_node_id_t destination);

/**
 * @brief Set the application-defined routing table lookup function.
 *
 * @param lookup_fn The lookup function.
 */
void deos_router_set_lookup_fn(deos_router_lookup_fn_t lookup_fn);

/**
 * @brief Feed an incoming message from a transport into the DEOS core dispatcher.
 *
 * @param msg The received message.
 * @param transport The transport interface it arrived on.
 */
void deos_feed_message(const deos_message_t *msg, deos_transport_t transport);

/**
 * @brief Feed a raw Universal DEOS Transport Frame from a transport into the DEOS core.
 *
 * This function decodes the frame into a deos_message_t, raises a fault if the frame
 * is malformed, and forwards it to the router or local dispatchers.
 *
 * @param frame_data Pointer to the raw universal frame buffer.
 * @param frame_len Length of the raw universal frame.
 * @param transport The transport interface it arrived on.
 * @return 0 on success, negative error code on decode/validation failure.
 */
int deos_feed_frame(const uint8_t *frame_data, size_t frame_len, deos_transport_t transport);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_H */

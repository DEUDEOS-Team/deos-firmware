/**
 * @file deos_registry.h
 * @brief Portable Message Handler Registry for DEOS Protocol
 *
 * Provides a reentrant, deterministic, zero-allocation callback table and
 * handler execution mechanism. Usable in both standalone Desktop/ROS 2 C++ nodes
 * and embedded firmware (Zephyr RTOS / bare-metal).
 */

#ifndef DEOS_REGISTRY_H
#define DEOS_REGISTRY_H

#include <deos/deos_types.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEOS_MAX_REGISTRY_HANDLERS
#define DEOS_MAX_REGISTRY_HANDLERS 32
#endif

/**
 * @brief Registry entry representing a single registered command listener.
 */
typedef struct {
    bool in_use;                         /**< Indicates if slot contains active callback */
    deos_node_id_t local_node;           /**< Target logical node ID (or 0xFF for wildcard) */
    deos_message_class_t message_class;  /**< Message class (COMMAND, STATUS, REQUEST, etc.) */
    deos_service_id_t service;           /**< Functional Service ID */
    uint8_t command;                     /**< Command ID */
    deos_message_handler_t handler;      /**< Callback function pointer */
    void *user_data;                     /**< Context pointer (e.g. C++ class instance) */
} deos_registry_entry_t;

/**
 * @brief Reentrant handler registry instance.
 */
typedef struct deos_registry {
    deos_registry_entry_t entries[DEOS_MAX_REGISTRY_HANDLERS];
    size_t count;
} deos_registry_t;

/**
 * @brief Initialize a registry instance (clears all entries).
 * @param reg Pointer to registry instance.
 * @return 0 on success, -EINVAL if reg is NULL.
 */
int deos_registry_init(deos_registry_t *reg);

/**
 * @brief Register a callback for a specific local node ID.
 *
 * @param reg           Registry instance.
 * @param local_node    Logical local node ID to match (e.g. DEOS_NODE_MAIN_STM32).
 * @param message_class Target message class.
 * @param service       Target service ID.
 * @param command       Target command ID.
 * @param handler       Function pointer to execute upon reception.
 * @param user_data     Optional context pointer passed to handler.
 *
 * @return 0 on success, -EEXIST if already registered, -ENOSPC if full, -EINVAL on bad args.
 */
int deos_registry_register_for_node(
    deos_registry_t *reg,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data);

/**
 * @brief Register a callback matching any destination node (wildcard 0xFF).
 *
 * @param reg           Registry instance.
 * @param message_class Target message class.
 * @param service       Target service ID.
 * @param command       Target command ID.
 * @param handler       Function pointer to execute upon reception.
 * @param user_data     Optional context pointer passed to handler.
 *
 * @return 0 on success, -EEXIST if already registered, -ENOSPC if full, -EINVAL on bad args.
 */
int deos_registry_register(
    deos_registry_t *reg,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data);

/**
 * @brief Unregister a previously registered callback.
 *
 * @param reg           Registry instance.
 * @param local_node    Local node ID.
 * @param message_class Message class.
 * @param service       Service ID.
 * @param command       Command ID.
 *
 * @return 0 on success, -ENOENT if not found, -EINVAL on bad args.
 */
int deos_registry_unregister(
    deos_registry_t *reg,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command);

/**
 * @brief Find registered handler for given parameters.
 *
 * @param reg           Registry instance.
 * @param local_node    Local node ID.
 * @param message_class Message class.
 * @param service       Service ID.
 * @param command       Command ID.
 * @param out_user_data Optional output pointer to retrieve user_data.
 *
 * @return Function pointer if found, NULL otherwise.
 */
deos_message_handler_t deos_registry_find(
    const deos_registry_t *reg,
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    void **out_user_data);

/**
 * @brief Match an incoming message against registered callbacks and execute them.
 *
 * @param reg Registry instance.
 * @param msg Pointer to incoming message.
 *
 * @return Number of handlers executed (>0 if matched), 0 if no handler, negative on error.
 */
int deos_registry_dispatch(
    const deos_registry_t *reg,
    const deos_message_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_REGISTRY_H */

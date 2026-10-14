#ifndef DEOS_FAULT_H
#define DEOS_FAULT_H

#include <stdint.h>
#include <stdbool.h>
#include <deos/deos_icd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Common Fault ID Registry
 * Node-specific fault IDs should start from 0x0100.
 */
typedef enum
{
    DEOS_FAULT_ID_END_OF_LIST             = 0x0000,
    DEOS_FAULT_INTERNAL_SOFTWARE_ERROR    = 0x0001,
    DEOS_FAULT_WATCHDOG_RESET             = 0x0002,
    DEOS_FAULT_INVALID_CONFIGURATION      = 0x0003,
    DEOS_FAULT_COMMUNICATION_TIMEOUT      = 0x0004,
    DEOS_FAULT_RX_QUEUE_OVERFLOW          = 0x0005,
    DEOS_FAULT_TX_FAILURE                 = 0x0006,
    DEOS_FAULT_PROTOCOL_ERROR             = 0x0007,
    DEOS_FAULT_LOCAL_STORAGE_ERROR        = 0x0008,

    /* Subscription Engine & Network Management Faults (0x0010 - 0x001F) */
    DEOS_FAULT_SUB_TABLE_FULL             = 0x0010,
    DEOS_FAULT_SUB_BROADCAST_REJECTED     = 0x0011,
    DEOS_FAULT_SUB_MALFORMED_PAYLOAD      = 0x0012,
    DEOS_FAULT_SUB_HANDLER_NOT_FOUND      = 0x0013,
    DEOS_FAULT_SUB_DISPATCH_FAILED        = 0x0014
} deos_common_fault_id_t;

/*
 * Fault Severity
 */
typedef enum
{
    DEOS_FAULT_SEVERITY_INFO     = 0x01,
    DEOS_FAULT_SEVERITY_WARNING  = 0x02,
    DEOS_FAULT_SEVERITY_ERROR    = 0x03,
    DEOS_FAULT_SEVERITY_CRITICAL = 0x04
} deos_fault_severity_t;

/*
 * Fault State
 */
typedef enum
{
    DEOS_FAULT_STATE_INACTIVE = 0x00,
    DEOS_FAULT_STATE_ACTIVE   = 0x01,
    DEOS_FAULT_STATE_LATCHED  = 0x02
} deos_fault_state_t;

/*
 * Fault Record Structure
 */
typedef struct
{
    uint16_t fault_id;
    uint8_t severity;
    uint8_t state;
    uint16_t occurrence_count;
    uint32_t last_occurrence_ms;
} deos_fault_record_t;

int deos_fault_raise_for_node(deos_node_id_t local_node, uint16_t fault_id, deos_fault_severity_t severity);
int deos_fault_set_inactive_for_node(deos_node_id_t local_node, uint16_t fault_id);
int deos_fault_latch_for_node(deos_node_id_t local_node, uint16_t fault_id);
int deos_fault_clear_for_node(deos_node_id_t local_node, uint16_t fault_id);
int deos_fault_clear_all_for_node(deos_node_id_t local_node);
bool deos_fault_is_active_for_node(deos_node_id_t local_node, uint16_t fault_id);

/**
 * @brief Primary node wrappers for backward compatibility
 */
int deos_fault_raise(uint16_t fault_id, deos_fault_severity_t severity);
int deos_fault_set_inactive(uint16_t fault_id);
int deos_fault_latch(uint16_t fault_id);
int deos_fault_clear(uint16_t fault_id);
int deos_fault_clear_all(void);
bool deos_fault_is_active(uint16_t fault_id);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_FAULT_H */

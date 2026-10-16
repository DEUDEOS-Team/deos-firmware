#ifndef DEOS_CAN_H
#define DEOS_CAN_H

#include <deos/deos_types.h>
#include <deos/deos_icd.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CAN Arbitration ID Codec (29-bit Extended ID)
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

/*
 * CAN-FD Frame Codec (DOC-ICD-CORE-001 Section 9.2 - CAN-FD Header)
 */
int deos_can_encode_frame(
    const deos_message_t *msg,
    struct can_frame *frame);

int deos_can_decode_frame(
    const struct can_frame *frame,
    deos_message_t *msg);

/**
 * @brief Initialize the DEOS CAN-FD Transport layer with a Zephyr CAN controller.
 *
 * Configures CAN-FD mode, configures hardware RX acceptance filters, and automatically
 * registers this transport with the DEOS core router or endpoint default transport.
 *
 * @param can_dev Pointer to the Zephyr CAN device.
 * @return 0 on success, negative error code on failure.
 */
int deos_can_transport_init(const struct device *can_dev);

/**
 * @brief Start CAN-FD background RX and TX worker threads and start the CAN peripheral.
 *
 * @return 0 on success, negative error code on failure.
 */
int deos_can_transport_start(void);

/**
 * @brief Transmit callback for DEOS router or direct transmission.
 *
 * Queues the message into the asynchronous CAN TX queue.
 *
 * @param msg The message to transmit over CAN-FD.
 * @return 0 on success, negative error code on failure.
 */
int deos_can_transport_tx(const deos_message_t *msg);

/**
 * @brief Check if CAN-FD transport is initialized and active.
 *
 * @return true if initialized, false otherwise.
 */
bool deos_can_transport_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_CAN_H */

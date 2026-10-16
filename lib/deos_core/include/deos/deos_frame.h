#ifndef DEOS_FRAME_H
#define DEOS_FRAME_H

#include <deos/deos_types.h>
#include <deos/deos_icd.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Universal DEOS Transport Frame (DOC-ICD-CORE-001 Section 8 / 9.1 - UNIVERSAL_9B)
 *
 * Byte 0: Destination Node (uint8_t)
 * Byte 1: Source Node (uint8_t)
 * Byte 2: Priority (uint8_t)
 * Byte 3: Version (uint8_t, 0x11 = v1.1)
 * Byte 4: Message Class (uint8_t)
 * Byte 5: Service ID (uint8_t)
 * Byte 6: Command ID (uint8_t)
 * Byte 7: Sequence (uint8_t)
 * Byte 8: Payload Length (uint8_t, N)
 * Byte 9..: Payload (uint8_t[N])
 */
#define DEOS_UNIVERSAL_HEADER_SIZE 9
#define DEOS_UF_DEST_OFFSET        0
#define DEOS_UF_SRC_OFFSET         1
#define DEOS_UF_PRIO_OFFSET        2
#define DEOS_UF_VER_OFFSET         3
#define DEOS_UF_CLASS_OFFSET       4
#define DEOS_UF_SERVICE_OFFSET     5
#define DEOS_UF_CMD_OFFSET         6
#define DEOS_UF_SEQ_OFFSET         7
#define DEOS_UF_LEN_OFFSET         8
#define DEOS_UF_PAYLOAD_OFFSET     9

/**
 * @brief Encode a deos_message_t into Media-Independent Universal DEOS Transport Frame (9+N bytes).
 *
 * @param msg Pointer to source deos_message_t
 * @param buf Output buffer
 * @param buf_max Maximum capacity of output buffer
 * @return Total frame length (9 + payload_len) on success, negative error code on failure.
 */
int deos_frame_encode(const deos_message_t *msg, uint8_t *buf, size_t buf_max);

/**
 * @brief Decode a Media-Independent Universal DEOS Transport Frame into deos_message_t.
 *
 * @param buf Input buffer containing Universal Frame bytes
 * @param frame_len Length of the received frame
 * @param msg Output deos_message_t structure
 * @return 0 on success, negative error code on failure.
 */
int deos_frame_decode(const uint8_t *buf, size_t frame_len, deos_message_t *msg);

/**
 * @brief Get human-readable string name of a DEOS node ID.
 *
 * @param node_id DEOS node ID
 * @return String representation of node
 */
const char *deos_node_name(uint8_t node_id);

/**
 * @brief Get human-readable string name of a DEOS transport ID.
 *
 * @param transport DEOS transport ID
 * @return String representation of transport
 */
const char *deos_transport_name(deos_transport_t transport);

/**
 * @brief Get human-readable string name of a DEOS service ID.
 *
 * @param service DEOS service ID
 * @return String representation of service
 */
const char *deos_service_name(uint8_t service);

/**
 * @brief Get human-readable string name of a DEOS message class.
 *
 * @param msg_class DEOS message class ID
 * @return String representation of class
 */
const char *deos_class_name(uint8_t msg_class);

/**
 * @brief Get human-readable string name of a DEOS command ID based on service.
 *
 * @param service DEOS service ID
 * @param cmd DEOS command ID
 * @return String representation of command
 */
const char *deos_command_name(uint8_t service, uint8_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_FRAME_H */

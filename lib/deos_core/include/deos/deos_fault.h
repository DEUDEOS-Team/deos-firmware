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

    /* --- Sistem & Donanım Hataları (0x0001 - 0x000F) --- */
    DEOS_FAULT_INTERNAL_SOFTWARE_ERROR    = 0x0001,
    DEOS_FAULT_WATCHDOG_RESET             = 0x0002,
    DEOS_FAULT_INVALID_CONFIGURATION      = 0x0003,
    DEOS_FAULT_COMMUNICATION_TIMEOUT      = 0x0004,
    DEOS_FAULT_RX_QUEUE_OVERFLOW          = 0x0005, /* Legacy generic alias */
    DEOS_FAULT_TX_FAILURE                 = 0x0006, /* Legacy generic alias */
    DEOS_FAULT_PROTOCOL_ERROR             = 0x0007, /* Legacy generic alias */
    DEOS_FAULT_LOCAL_STORAGE_ERROR        = 0x0008,
    DEOS_FAULT_ESTOP_TRIGGERED            = 0x0009,
    DEOS_FAULT_MEMORY_ALLOCATION_FAILED   = 0x000A,

    /* --- Telemetri & Abonelik Hataları (0x0010 - 0x001F) --- */
    DEOS_FAULT_SUB_TABLE_FULL             = 0x0010,
    DEOS_FAULT_SUB_BROADCAST_REJECTED     = 0x0011,
    DEOS_FAULT_SUB_MALFORMED_PAYLOAD      = 0x0012,
    DEOS_FAULT_SUB_HANDLER_NOT_FOUND      = 0x0013,
    DEOS_FAULT_SUB_DISPATCH_FAILED        = 0x0014,

    /* --- Yönlendirici (Router) Hataları (0x0020 - 0x002F) --- */
    DEOS_FAULT_ROUTER_NO_POLICY           = 0x0020, /* Rota politikası fonksiyonu atanmamış */
    DEOS_FAULT_ROUTER_UNMAPPED_ROUTE      = 0x0021, /* Hedef düğüm rota tablosunda yok (DROP) */
    DEOS_FAULT_ROUTER_TRP_UNAVAILABLE     = 0x0022, /* Hedef hat bağlı değil veya TX kaydı yok */
    DEOS_FAULT_ROUTER_LOOPBACK_DETECTED   = 0x0023, /* Paket geldiği hatta geri gönderilmek istendi */
    DEOS_FAULT_ROUTER_QUEUE_FULL          = 0x0024, /* Router kuyruğu taştı */

    /* --- Protokol & Çerçeve Hataları (0x0030 - 0x003F) --- */
    DEOS_FAULT_PROTOCOL_VERSION_MISMATCH  = 0x0030, /* Protokol versiyonu uyumsuz */
    DEOS_FAULT_FRAME_HEADER_TOO_SHORT     = 0x0031, /* Evrensel başlık boyutu yetersiz */
    DEOS_FAULT_PAYLOAD_TOO_LARGE          = 0x0032, /* Payload DEOS_MAX_PAYLOAD_LEN'i aşıyor */
    DEOS_FAULT_FRAME_CRC_MISMATCH         = 0x0033, /* CRC doğrulama hatası */
    DEOS_FAULT_FRAME_DECODE_FAILED        = 0x0034, /* Çerçeve decode edilemedi */

    /* --- CAN / CAN-FD Taşıyıcı Hataları (0x0040 - 0x005F) --- */
    DEOS_FAULT_CAN_DEV_NOT_READY          = 0x0040, /* CAN donanım aygıtı hazır değil */
    DEOS_FAULT_CAN_MODE_NOT_SUPPORTED     = 0x0041, /* CAN-FD modu donanımca desteklenmiyor */
    DEOS_FAULT_CAN_FILTER_INSTALL_FAILED  = 0x0042, /* Donanımsal RX filtre kurulamadı */
    DEOS_FAULT_CAN_RX_QUEUE_FULL          = 0x0043, /* CAN RX kuyruğu taştı */
    DEOS_FAULT_CAN_TX_QUEUE_FULL          = 0x0044, /* CAN TX kuyruğu taştı */
    DEOS_FAULT_CAN_TX_SEND_FAILED         = 0x0045, /* Zephyr can_send başarısız oldu / timeout */
    DEOS_FAULT_CAN_FRAME_DECODE_ERR       = 0x0046, /* Gelen CAN frame parse edilemedi */
    DEOS_FAULT_CAN_BUS_OFF                = 0x0047, /* CAN controller Bus-Off durumunda */

    /* --- UART / LoRa Taşıyıcı Hataları (0x0060 - 0x007F) --- */
    DEOS_FAULT_LORA_DEV_NOT_READY         = 0x0060, /* LoRa UART donanımı hazır değil */
    DEOS_FAULT_LORA_TX_QUEUE_FULL         = 0x0061, /* LoRa TX kuyruğu taştı */
    DEOS_FAULT_LORA_RX_BUFFER_OVERFLOW    = 0x0062, /* LoRa ring buffer taştı */
    DEOS_FAULT_LORA_CRC_MISMATCH          = 0x0063, /* Havadan gelen paketin CRC'si hatalı */
    DEOS_FAULT_LORA_FRAME_MALFORMED       = 0x0064, /* Havadan gelen paket başlığı bozuk */
    DEOS_FAULT_LORA_ACK_TIMEOUT           = 0x0065, /* ACK beklenen pakete yanıt gelmedi */

    /* --- Ethernet / Network Taşıyıcı Hataları (0x0080 - 0x009F) --- */
    DEOS_FAULT_ETH_LINK_DOWN              = 0x0080, /* Ethernet kablosu takılı değil / Link yok */
    DEOS_FAULT_ETH_SOCKET_CREATE_FAILED   = 0x0081, /* TCP/UDP socket oluşturulamadı */
    DEOS_FAULT_ETH_SOCKET_BIND_FAILED     = 0x0082, /* Socket porta bind edilemedi */
    DEOS_FAULT_ETH_TX_QUEUE_FULL          = 0x0083, /* Ethernet TX kuyruğu taştı */
    DEOS_FAULT_ETH_TX_SEND_FAILED         = 0x0084, /* Socket send/sendto hatası */
    DEOS_FAULT_ETH_CLIENT_DISCONNECTED    = 0x0085, /* Bağlı UI / Ros Bridge istemcisi koptu */
    DEOS_FAULT_ETH_FRAME_DECODE_ERR       = 0x0086, /* Gelen Ethernet çerçevesi bozuk */

    /* --- Dağıtıcı (Dispatcher) Hataları (0x00A0 - 0x00AF) --- */
    DEOS_FAULT_DISPATCH_TABLE_FULL        = 0x00A0, /* Handler tablosunda yer kalmadı */
    DEOS_FAULT_DISPATCH_DUPLICATE_HANDLER = 0x00A1, /* Aynı komut için mükerrer handler kaydı */
    DEOS_FAULT_DISPATCH_HANDLER_NOT_FOUND = 0x00A2  /* Gelen komut için handler tanımlı değil */
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

/**
 * @brief Get human-readable string name of a DEOS fault ID.
 *
 * @param fault_id The fault identifier.
 * @return String representation of the fault.
 */
const char *deos_fault_name(uint16_t fault_id);

/**
 * @brief Get human-readable string name of a fault severity.
 *
 * @param severity The fault severity enum.
 * @return String representation of severity (INFO, WARNING, ERROR, CRITICAL).
 */
const char *deos_fault_severity_name(deos_fault_severity_t severity);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_FAULT_H */

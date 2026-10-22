#ifndef DUMMY_CAN_FAULT_LIST_H
#define DUMMY_CAN_FAULT_LIST_H

#include <deos/deos_fault.h>

/*
 * DEOS Common Fault IDs - Unified Mirror for Dummy CAN & Testing
 * Source: lib/deos_core/include/deos/deos_fault.h
 */

typedef struct {
    uint16_t fault_id;
    const char *name;
    const char *category;
    const char *description;
} deos_fault_desc_t;

static const deos_fault_desc_t DEOS_COMMON_FAULT_TABLE[] = {
    /* Sistem & Donanım */
    { DEOS_FAULT_ID_END_OF_LIST,             "END_OF_LIST",             "SYSTEM",    "Reserved -- Fault listesi sonu" },
    { DEOS_FAULT_INTERNAL_SOFTWARE_ERROR,    "INTERNAL_SOFTWARE_ERROR", "SYSTEM",    "Dahili yazilim / assertion hatasi" },
    { DEOS_FAULT_WATCHDOG_RESET,             "WATCHDOG_RESET",          "SYSTEM",    "Watchdog reset algilandi" },
    { DEOS_FAULT_INVALID_CONFIGURATION,      "INVALID_CONFIGURATION",   "SYSTEM",    "Gecersiz baslangic konfigurasyonu" },
    { DEOS_FAULT_COMMUNICATION_TIMEOUT,      "COMMUNICATION_TIMEOUT",   "SYSTEM",    "Genel iletisim zaman asimi" },
    { DEOS_FAULT_LOCAL_STORAGE_ERROR,        "LOCAL_STORAGE_ERROR",     "SYSTEM",    "Dahili hafiza / NVM erisim hatasi" },
    { DEOS_FAULT_ESTOP_TRIGGERED,            "ESTOP_TRIGGERED",         "SYSTEM",    "Acil durdurma (E-STOP) tetiklendi" },
    { DEOS_FAULT_MEMORY_ALLOCATION_FAILED,   "MEMORY_ALLOC_FAILED",     "SYSTEM",    "Dinamik/statik bellek tahsis hatasi" },

    /* Telemetri & Abonelik */
    { DEOS_FAULT_SUB_TABLE_FULL,             "SUB_TABLE_FULL",          "SUB",       "Abonelik tablosu kapasite asimi" },
    { DEOS_FAULT_SUB_BROADCAST_REJECTED,     "SUB_BROADCAST_REJECTED",  "SUB",       "Broadcast hedefine abonelik reddedildi" },
    { DEOS_FAULT_SUB_MALFORMED_PAYLOAD,      "SUB_MALFORMED_PAYLOAD",   "SUB",       "Abonelik istek paketi bozuk / gecersiz boyut" },
    { DEOS_FAULT_SUB_HANDLER_NOT_FOUND,      "SUB_HANDLER_NOT_FOUND",   "SUB",       "Abone olunmak istenen handler kayitli degil" },
    { DEOS_FAULT_SUB_DISPATCH_FAILED,        "SUB_DISPATCH_FAILED",     "SUB",       "Sentetik abonelik periyot tetiklemesi basarisiz" },

    /* Yönlendirici (Router) */
    { DEOS_FAULT_ROUTER_NO_POLICY,           "ROUTER_NO_POLICY",        "ROUTER",    "Rota politikasi fonksiyonu atanmamis" },
    { DEOS_FAULT_ROUTER_UNMAPPED_ROUTE,      "ROUTER_UNMAPPED_ROUTE",   "ROUTER",    "Hedef dugum rota tablosunda yok (DROP)" },
    { DEOS_FAULT_ROUTER_TRP_UNAVAILABLE,     "ROUTER_TRP_UNAVAILABLE",  "ROUTER",    "Hedef hat bagli degil veya TX kaydi yok" },
    { DEOS_FAULT_ROUTER_LOOPBACK_DETECTED,   "ROUTER_LOOPBACK_DETECTED","ROUTER",    "Paket geldigi hatta geri gonderilmek istendi" },
    { DEOS_FAULT_ROUTER_QUEUE_FULL,          "ROUTER_QUEUE_FULL",       "ROUTER",    "Router iletim kuyrugu tasti" },

    /* Protokol & Çerçeve */
    { DEOS_FAULT_PROTOCOL_VERSION_MISMATCH,  "PROTO_VERSION_MISMATCH",  "PROTOCOL",  "Protokol versiyonu uyumsuz" },
    { DEOS_FAULT_FRAME_HEADER_TOO_SHORT,     "FRAME_HEADER_TOO_SHORT",  "PROTOCOL",  "Evrensel baslik boyutu yetersiz" },
    { DEOS_FAULT_PAYLOAD_TOO_LARGE,          "PAYLOAD_TOO_LARGE",       "PROTOCOL",  "Payload DEOS_MAX_PAYLOAD_LEN'i asiyor" },
    { DEOS_FAULT_FRAME_CRC_MISMATCH,         "FRAME_CRC_MISMATCH",      "PROTOCOL",  "Cerceve CRC dogrulama hatasi" },
    { DEOS_FAULT_FRAME_DECODE_FAILED,        "FRAME_DECODE_FAILED",     "PROTOCOL",  "Cerceve decode edilemedi / bozuk paket" },

    /* CAN / CAN-FD Taşıyıcı */
    { DEOS_FAULT_CAN_DEV_NOT_READY,          "CAN_DEV_NOT_READY",       "CAN_FD",    "CAN donanim aygiti hazir degil" },
    { DEOS_FAULT_CAN_MODE_NOT_SUPPORTED,     "CAN_MODE_NOT_SUPPORTED",  "CAN_FD",    "CAN-FD modu donanimca desteklenmiyor" },
    { DEOS_FAULT_CAN_FILTER_INSTALL_FAILED,  "CAN_FILTER_INSTALL_FAIL", "CAN_FD",    "Donanimsal RX filtre kurulamadi" },
    { DEOS_FAULT_CAN_RX_QUEUE_FULL,          "CAN_RX_QUEUE_FULL",       "CAN_FD",    "CAN RX kuyrugu tasti" },
    { DEOS_FAULT_CAN_TX_QUEUE_FULL,          "CAN_TX_QUEUE_FULL",       "CAN_FD",    "CAN TX kuyrugu tasti" },
    { DEOS_FAULT_CAN_TX_SEND_FAILED,         "CAN_TX_SEND_FAILED",      "CAN_FD",    "Zephyr can_send basarisiz oldu / timeout" },
    { DEOS_FAULT_CAN_FRAME_DECODE_ERR,       "CAN_FRAME_DECODE_ERR",    "CAN_FD",    "Gelen CAN frame parse edilemedi" },
    { DEOS_FAULT_CAN_BUS_OFF,                "CAN_BUS_OFF",             "CAN_FD",    "CAN controller Bus-Off durumunda" },

    /* UART / LoRa Taşıyıcı */
    { DEOS_FAULT_LORA_DEV_NOT_READY,         "LORA_DEV_NOT_READY",      "UART_LORA", "LoRa UART donanimi hazir degil" },
    { DEOS_FAULT_LORA_TX_QUEUE_FULL,         "LORA_TX_QUEUE_FULL",      "UART_LORA", "LoRa TX kuyrugu tasti" },
    { DEOS_FAULT_LORA_RX_BUFFER_OVERFLOW,    "LORA_RX_BUF_OVERFLOW",    "UART_LORA", "LoRa ring buffer tasti" },
    { DEOS_FAULT_LORA_CRC_MISMATCH,          "LORA_CRC_MISMATCH",       "UART_LORA", "Havadan gelen paketin CRC'si hatali" },
    { DEOS_FAULT_LORA_FRAME_MALFORMED,       "LORA_FRAME_MALFORMED",    "UART_LORA", "Havadan gelen paket basligi bozuk" },
    { DEOS_FAULT_LORA_ACK_TIMEOUT,           "LORA_ACK_TIMEOUT",        "UART_LORA", "ACK beklenen pakete yanit gelmedi" },

    /* Ethernet / Network Taşıyıcı */
    { DEOS_FAULT_ETH_LINK_DOWN,              "ETH_LINK_DOWN",           "ETHERNET",  "Ethernet kablosu takili degil / Link yok" },
    { DEOS_FAULT_ETH_SOCKET_CREATE_FAILED,   "ETH_SOCK_CREATE_FAILED",  "ETHERNET",  "TCP/UDP socket olusturulamadi" },
    { DEOS_FAULT_ETH_SOCKET_BIND_FAILED,     "ETH_SOCK_BIND_FAILED",    "ETHERNET",  "Socket porta bind edilemedi" },
    { DEOS_FAULT_ETH_TX_QUEUE_FULL,          "ETH_TX_QUEUE_FULL",       "ETHERNET",  "Ethernet TX kuyrugu tasti" },
    { DEOS_FAULT_ETH_TX_SEND_FAILED,         "ETH_TX_SEND_FAILED",      "ETHERNET",  "Socket send/sendto hatasi" },
    { DEOS_FAULT_ETH_CLIENT_DISCONNECTED,    "ETH_CLIENT_DISCONNECTED", "ETHERNET",  "Bagli UI / Ros Bridge istemcisi koptu" },
    { DEOS_FAULT_ETH_FRAME_DECODE_ERR,       "ETH_FRAME_DECODE_ERR",    "ETHERNET",  "Gelen Ethernet cercevesi bozuk" },

    /* Dağıtıcı (Dispatcher) */
    { DEOS_FAULT_DISPATCH_TABLE_FULL,        "DISPATCH_TABLE_FULL",     "DISPATCH",  "Handler tablosunda yer kalmadi" },
    { DEOS_FAULT_DISPATCH_DUPLICATE_HANDLER, "DISPATCH_DUP_HANDLER",    "DISPATCH",  "Ayni komut icin mukerrer handler kaydi" },
    { DEOS_FAULT_DISPATCH_HANDLER_NOT_FOUND, "DISPATCH_HANDLER_NOT_FOUND","DISPATCH","Gelen komut icin handler tanimli degil" }
};

#endif /* DUMMY_CAN_FAULT_LIST_H */

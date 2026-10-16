#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(deos_dispatch, LOG_LEVEL_INF);



/**
 * @brief Kayitli uygulama mesaj isleyicisi (handler) yapisi.
 * Tablodaki her bir slot, belirli bir yerel dugumun dinledigi komutu ve
 * tetiklenecek fonksiyon gostericisini tutar.
 */
struct deos_handler_entry {
    bool in_use;                         /**< Bu slot su an aktif bir kayit iceriyor mu? */
    deos_node_id_t local_node;           /**< Bu isleyiciyi kaydeden yerel mantiksal dugum ID'si */
    deos_message_class_t message_class;  /**< Dinlenen mesaj sinifi (COMMAND, REQUEST, EVENT vb.) */
    deos_service_id_t service;           /**< Dinlenen servis ID'si (SYSTEM, TRACTION, STEERING vb.) */
    uint8_t command;                     /**< Dinlenen komut numarasi (orn: 0x01 HIZ_VER) */
    deos_message_handler_t handler;      /**< Mesaj geldiginde calistirilacak callback fonksiyonu */
    void *user_data;                     /**< Fonksiyona iletilecek kullanici ozel verisi (context pointer) */
};

/** @brief Sistemde ayni anda kayitli olabilecek maksimum isleyici tablosu */
static struct deos_handler_entry handler_registry[DEOS_MAX_HANDLERS];

/**
 * @brief Dispatcher (mesaj dagitim) tablosunu baslatir ve sifirlar.
 * 
 * Sistem ilk acildiginda cagirilir. Tablodaki tum slotlari bosaltir.
 * 
 * @return 0 Her zaman basarili.
 */
int deos_dispatch_init(void)
{
    memset(handler_registry, 0, sizeof(handler_registry));
    LOG_DBG("Dispatcher initialized");
    return 0;
}

/**
 * @brief Belirli bir yerel mantiksal dugum icin mesaj isleyici (callback) kaydeder.
 * 
 * Eger tek bir STM32 icinde birden fazla dugum barindiriliyorsa (hosted nodes),
 * hangi dugumun bu komutu dinledigi 'local_node' ile belirtilir.
 * 
 * @param local_node    Komutu dinleyecek yerel dugum ID'si (orn: DEOS_NODE_MAIN_STM32).
 * @param message_class Mesaj sinifi (DEOS_CLASS_COMMAND, DEOS_CLASS_REQUEST vb.).
 * @param service       Servis ID'si (DEOS_SERVICE_TRACTION, DEOS_SERVICE_BRAKE vb.).
 * @param command       Komut kodu (orn: 0x01).
 * @param handler       Mesaj geldiginde tetiklenecek C/C++ fonksiyonu.
 * @param user_data     Fonksiyona parametre olarak gecilecek nesne/veri isaretcisi (opsiyonel).
 * 
 * @return 0 Basarili, -EINVAL Gecersiz handler, -EPERM Dugum yerel degil,
 *         -EEXIST Bu komut zaten kayitli, -ENOSPC Tablo tamamen dolu.
 */
int deos_register_handler_for_node(
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    if (!handler) {
        return -EINVAL;
    }

    if (!deos_is_local_node(local_node)) {
        return -EPERM;
    }

    int free_slot = -1;

    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (!handler_registry[i].in_use) {
            if (free_slot == -1) {
                free_slot = i;
            }
        } else {
            if (handler_registry[i].local_node == local_node &&
                handler_registry[i].message_class == message_class &&
                handler_registry[i].service == service &&
                handler_registry[i].command == command) {
                return -EEXIST; /* Duplicate */
            }
        }
    }

    if (free_slot == -1) {
        return -ENOSPC; /* Full */
    }

    handler_registry[free_slot].local_node    = local_node;
    handler_registry[free_slot].message_class = message_class;
    handler_registry[free_slot].service       = service;
    handler_registry[free_slot].command       = command;
    handler_registry[free_slot].handler       = handler;
    handler_registry[free_slot].user_data     = user_data;
    handler_registry[free_slot].in_use        = true;

    LOG_DBG("Registered handler for Node: 0x%02X Class: 0x%X, Svc: 0x%X, Cmd: 0x%X",
            local_node, message_class, service, command);

    return 0;
}

/**
 * @brief Varsayilan (birincil) yerel dugum icin mesaj isleyici kaydeder.
 * 
 * deos_register_handler_for_node() fonksiyonunun pratik bir sarmalayicisidir (wrapper).
 * Cihazin varsayilan node_id'sini sistem konfigürasyonundan otomatik alir.
 * 
 * @param message_class Mesaj sinifi (COMMAND, REQUEST vb.).
 * @param service       Servis ID'si (TRACTION, STEERING vb.).
 * @param command       Komut kodu.
 * @param handler       Tetiklenecek callback fonksiyonu.
 * @param user_data     Kullanici ozel verisi (opsiyonel).
 * 
 * @return 0 Basarili, negatif hata kodu basarisiz.
 */
int deos_register_handler(
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_register_handler_for_node(
        config->node_id, message_class, service, command, handler, user_data);
}

/**
 * @brief Tabloda belirli bir dugum ve komut icin kayitli olan isleyiciyi arar.
 * 
 * Genellikle testlerde veya dinamik kontrolde "bu komutu dinleyen fonksiyon var mi?"
 * diye sorgulamak icin kullanilir.
 * 
 * @param local_node    Aranan dugum ID'si.
 * @param message_class Aranan mesaj sinifi.
 * @param service       Aranan servis ID'si.
 * @param command       Aranan komut kodu.
 * @param out_user_data Kayitli user_data'yi disari aktarmak icin cikis gostericisi (opsiyonel).
 * 
 * @return Bulunursa fonksiyon isaretcisi, bulunamazsa NULL.
 */
deos_message_handler_t deos_dispatch_find_handler(
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    void **out_user_data)
{
    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (handler_registry[i].in_use &&
            handler_registry[i].local_node == local_node &&
            handler_registry[i].message_class == message_class &&
            handler_registry[i].service == service &&
            handler_registry[i].command == command) {
            if (out_user_data) {
                *out_user_data = handler_registry[i].user_data;
            }
            return handler_registry[i].handler;
        }
    }
    return NULL;
}

/**
 * @brief Bu mikrokontrolcuye (yerel dugumlere) ulasan bir paketi iceride isler.
 * 
 * Bu fonksiyon sirasiyla su asamalarla calisir:
 * 1. Cekirdek Ag Komutlari: PING veya PONG gibi DEOS altyapi mesajlari otomatik yanitlanir.
 * 2. Cekirdek Abonelik Komutlari: SUBSCRIBE / UNSUBSCRIBE telemetri istekleri islenir.
 * 3. Tani (Diagnostic) Komutlari: Hata listesi sorgulama (GET_FAULTS) veya temizleme (CLEAR_FAULTS) yapilir.
 * 4. Uygulama Isleyicileri: Yukaridaki genel sistem mesajlari degilse, tablodaki uygulama
 *    callback'leri taranir. Eger mesaj Unicast ise ilk eslesmede cikar; Broadcast ise
 *    ayni islemci icindeki o yayini dinleyen tum yerel dugumlerin fonksiyonlarini cagirir.
 * 
 * @param msg Islenecek cozulmus DEOS mesaji.
 */
void deos_dispatch_local(const deos_message_t *msg)
{
    if (!msg) {
        return;
    }

    /* Common Network Behaviour */
    if (msg->message_class == DEOS_CLASS_NETWORK && msg->service == DEOS_SERVICE_SYSTEM) {
        switch (msg->command) {
            case DEOS_CMD_SYSTEM_PING:
                deos_handle_ping(msg);
                return;
            case DEOS_CMD_SYSTEM_PONG:
                deos_handle_pong(msg);
                return;
            case DEOS_CMD_SYSTEM_HEARTBEAT:
                /* Do not auto-consume heartbeat, let application handle it */
                break;
            default:
                break; /* Allow app to handle other system network cmds */
        }
    }

    /* Common Subscription Behaviour */
    if (msg->service == DEOS_SERVICE_SYSTEM && msg->message_class == DEOS_CLASS_REQUEST) {
        if (msg->command == DEOS_CMD_SYSTEM_SUBSCRIBE) {
            deos_subscription_handle_subscribe(msg);
            return;
        } else if (msg->command == DEOS_CMD_SYSTEM_UNSUBSCRIBE) {
            deos_subscription_handle_unsubscribe(msg);
            return;
        }
    }

    /* Common Diagnostic Behaviour */
    if (msg->service == DEOS_SERVICE_DIAGNOSTIC) {
        if (msg->message_class == DEOS_CLASS_REQUEST && msg->command == DEOS_CMD_DIAG_GET_FAULTS) {
            deos_fault_handle_get_faults(msg);
            return;
        } else if (msg->message_class == DEOS_CLASS_COMMAND && msg->command == DEOS_CMD_DIAG_CLEAR_FAULTS) {
            deos_fault_handle_clear_faults(msg);
            return;
        }
    }

    /* Application Handlers */
    bool handled = false;
    bool is_broadcast = (msg->destination == DEOS_NODE_GLOBAL_BROADCAST || 
                         msg->destination == DEOS_NODE_CAN_BROADCAST);

    for (int i = 0; i < DEOS_MAX_HANDLERS; i++) {
        if (handler_registry[i].in_use &&
            (handler_registry[i].local_node == msg->destination || is_broadcast) &&
            handler_registry[i].message_class == msg->message_class &&
            handler_registry[i].service == msg->service &&
            handler_registry[i].command == msg->command) {
            
            int ret = handler_registry[i].handler(msg, handler_registry[i].user_data);
            if (ret != 0) {
                LOG_WRN("Handler returned error %d for Cmd: 0x%X (Node: 0x%02X)",
                        ret, msg->command, handler_registry[i].local_node);
            }
            handled = true;

            /* Unicast mesajlar tek bir dugume aittir, ilk eslesmede donguyu bitir.
             * Broadcast mesajlar ayni islemcideki tum dinleyici dugumlere dagitilmalidir. */
            if (!is_broadcast) {
                break;
            }
        }
    }

    if (!handled) {
        LOG_DBG("No handler for Class: 0x%X, Svc: 0x%X, Cmd: 0x%X",
                msg->message_class, msg->service, msg->command);
    }
}

/**
 * @brief Gelen bir mesaji dugumun rolune (Router veya Endpoint) gore dagitir.
 * 
 * - Eger cihaz bir Router (Yonlendirici) ise: Mesaj `deos_route_message()`'a gonderilir.
 *   Router hem yerel dugumleri kontrol eder hem de diger hatlara iletir (forward eder).
 * - Eger cihaz normal bir Uc Dugum (Endpoint) ise: Sadece mesaji yerel dugum ID'si
 *   veya Broadcast ile eslesiyorsa `deos_dispatch_local()` ile iceride isler; diger
 *   dugumlere ait mesajlari gormezden gelir.
 * 
 * @param msg                Islenecek cozulmus DEOS mesaji.
 * @param incoming_transport Mesajin bu mikrokontrolcuye geldigi fiziksel hat (CAN, LoRa vb.).
 */
void deos_dispatch(const deos_message_t *msg, deos_transport_t incoming_transport)
{
    if (!msg) {
        return;
    }

    const struct deos_config *config = deos_get_config();
    if (config && config->router_enabled) {
        deos_route_message(msg, incoming_transport);
    } else {
        /* Non-router endpoint node: process incoming message only if local or broadcast */
        if (deos_is_local_node(msg->destination) || 
            msg->destination == DEOS_NODE_GLOBAL_BROADCAST || 
            msg->destination == DEOS_NODE_CAN_BROADCAST) {
            deos_dispatch_local(msg);
        }
    }
}

/**
 * @brief Cozulmus hazir bir mesaji dagitim sistemine besler (feed eder).
 * 
 * Dahili veya simule edilmis mesajlarin sisteme sokulmasi icin kullanilir.
 * 
 * @param msg       DEOS mesaji.
 * @param transport Mesajin geldigi hat.
 */
void deos_feed_message(const deos_message_t *msg, deos_transport_t transport)
{
    if (msg) {
        deos_dispatch(msg, transport);
    }
}

/**
 * @brief Fiziksel hattan gelen ham byte paketini (frame) sisteme besler.
 * 
 * CAN-FD, LoRa veya Ethernet suruculeri disaridan ham byte dizisi aldiklarinda
 * bu fonksiyonu cagirirlar.
 * Fonksiyon sirasiyla:
 * 1. Ham veriyi ve CRC'yi deşifre eder (`deos_frame_decode`).
 * 2. Eger paket bozuksa hata uretir (`DEOS_FAULT_FRAME_DECODE_FAILED`).
 * 3. Paket saglamsa `deos_dispatch()` cagirarak yonlendirme/dagitim zincirini baslatir.
 * 
 * @param frame_data Ham byte dizisi.
 * @param frame_len  Ham verinin uzunlugu (bayt).
 * @param transport  Paketin geldigi donanim hatti (DEOS_TRANSPORT_CAN, UART_LORA vb.).
 * 
 * @return 0 Basarili, negatif hata kodu basarisiz / bozuk paket.
 */
int deos_feed_frame(const uint8_t *frame_data, size_t frame_len, deos_transport_t transport)
{
    if (!frame_data) {
        return -EINVAL;
    }

    deos_message_t msg;
    int ret = deos_frame_decode(frame_data, frame_len, &msg);
    if (ret != 0) {
        LOG_WRN("DEOS RX Error: Corrupt frame (%d) via %s", ret, deos_transport_name(transport));
        deos_fault_raise(DEOS_FAULT_FRAME_DECODE_FAILED, DEOS_FAULT_SEVERITY_WARNING);
        return ret;
    }

    deos_dispatch(&msg, transport);
    return 0;
}

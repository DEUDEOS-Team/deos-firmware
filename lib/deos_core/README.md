# DEOS Core Protocol Stack & Kullanım Kılavuzu

**DEOS Core**, Zephyr RTOS üzerinde koşan dağıtık STM32 mikrodenetleyici düğümleri (nodes) için özel olarak geliştirilmiş hafif (lightweight), deterministik, sıfır dinamik bellek (zero-malloc) kullanan araç içi haberleşme ve yönlendirme protokol yığınıdır.

Bu kılavuz, DEOS Core kütüphanesini projelerinize nasıl entegre edeceğinizi, uç düğüm (Endpoint) ve yönlendirici (Router/Gateway) mimarilerini nasıl kuracağınızı ve protokol yığınının sunduğu tüm API'leri nasıl kullanacağınızı adım adım açıklar.

---

## 📑 İçindekiler
1. [Temel Mimari Özellikler](#-temel-mimari-özellikler)
2. [Haberleşme & Çerçeve Formatları](#-haberleşme--çerçeve-formatları)
3. [Projeye Dahil Etme (Entegrasyon)](#-projeye-dahil-etme-entegrasyon)
4. [Kullanım Kılavuzu: Uç Düğüm (Endpoint Node) Oluşturma](#-kullanım-kılavuzu-uç-düğüm-endpoint-node-oluşturma)
5. [Kullanım Kılavuzu: Çoklu Mantıksal Düğüm (Hosted Nodes)](#-kullanım-kılavuzu-çoklu-mantıksal-düğüm-hosted-nodes)
6. [Kullanım Kılavuzu: Yönlendirici / Ağ Geçidi (Router Node) Oluşturma](#-kullanım-kılavuzu-yönlendirici--ağ-geçidi-router-node-oluşturma)
7. [Telemetri & Abonelik Sistemi (Pub/Sub)](#-telemetri--abonelik-sistemi-pubsub)
8. [Düğüm Bazlı Hata Yönetimi (Fault Management)](#-düğüm-bazlı-hata-yönetimi-fault-management)
9. [C++ Nesne Yönelimli Entegrasyon Örneği](#-c-nesne-yönelimli-entegrasyon-örneği)
10. [Sık Yapılan Hatalar ve Önemli Kurallar](#-sık-yapılan-hatalar-ve-önemli-kurallar)

---

## 🏗 Temel Mimari Özellikler

- **Sıfır Dinamik Bellek (Zero Dynamic Memory):** Heap veya `malloc`/`free` asla kullanılmaz. Tüm kuyruklar, buffer'lar ve context yapıları derleme zamanında static/BSS üzerinde tahsis edilir.
- **Katmanlı ve Soyutlanmış Transport:** Çekirdek protokol (`deos_core`), fiziksel hatlardan bağımsızdır. CAN-FD (`lib/deos_transport_can`), UART/LoRa veya Ethernet arayüzleri bağımsız birer eklenti (plug-in) olarak sisteme beslenir (`deos_feed_frame`).
- **Hosted Local Nodes (Çoklu Mantıksal Düğüm):** Tek bir fiziksel STM32 üzerinde birden fazla mantıksal düğüm (örneğin hem `TRACTION_LEFT` hem `TRACTION_RIGHT`) barındırılabilir. Her düğümün `Sequence Counter`'ı, `Fault Table`'ı ve abonelikleri bağımsızdır.
- **Akıllı Broadcast Dağıtımı:** Tek bir işlemcide birden fazla mantıksal düğüm barındığında, dışarıdan gelen bir Acil Durum (`E-STOP`) veya genel yayın mesajı işlemci içindeki o yayını dinleyen tüm düğümlere eksiksiz dağıtılır.
- **Otonom PING/PONG & Heartbeat:** Uygulama müdahalesi gerekmeksizin ağ canlılık ping'lerine otomatik yanıt verir.
- **Pub/Sub Telemetri Motoru:** Düğümler arası periyodik (10ms, 100ms, 1000ms vb.) veya durum değişimine bağlı (On-Change) veri akışını TTL kira mekanizmasıyla yönetir.

---

## 📡 Haberleşme & Çerçeve Formatları

### 1. 29-bit CAN ID Formatı (Extended Identifier)

CAN-FD hattı üzerinden taşınan paketlerde 29-bit Extended CAN ID şu şekilde çözümlenir:

| Bit Aralığı | Uzunluk | Alan Adı | Açıklama |
| :--- | :---: | :--- | :--- |
| **28..26** | 3 bit | **Priority** | Mesaj önceliği (`EMERGENCY=0x00`, `SAFETY=0x01`, `COMMAND=0x02`, `STATUS=0x04`) |
| **25..22** | 4 bit | **Message Class** | Mesaj sınıfı (`COMMAND=0x01`, `STATUS=0x02`, `EVENT=0x03`, `REQUEST=0x04`, `RESPONSE=0x05`, `DIAGNOSTIC=0x06`, `NETWORK=0x07`) |
| **21..16** | 6 bit | **Service ID** | Hedef Servis (`SYSTEM=0x00`, `TRACTION=0x01`, `STEERING=0x02`, `BRAKE=0x03`, `BMS=0x04` vb.) |
| **15..8**  | 8 bit | **Destination Node** | Hedef Düğüm ID (`0xFF`: Global Broadcast, `0xFE`: CAN Broadcast) |
| **7..0**   | 8 bit | **Source Node** | Gönderen Kaynak Düğüm ID |

---

### 2. Ortak Veri Başlığı (Common Header - 4 Bayt)

Her DEOS mesajının payload başında 4 baytlık ortak başlık bulunur:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Version    |   Sequence    |    Command    | Payload Length|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Command Payload (0 .. 60 Bytes)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

* `Version (0x11)`: Protokol sürümü (v1.1).
* `Sequence`: Düğümün gönderdiği her mesajda 1 artan sıra sayacı (paket kaybı tespiti için).
* `Command`: Servise ait komut ID'si (ör. `0x01`).
* `Payload Length`: Başlık sonrası gelen net veri uzunluğu (Maksimum 60 bayt).

---

### 3. Evrensel Çerçeve (Universal Transport Frame)

Stream tabanlı hatlar (LoRa UART, Ethernet TCP/UDP vb.) üzerinden taşınırken başa Magic Baytları ve sona CRC16 eklenir:

```
[0xDE] [0x05] [Len: 1B] [Priority: 1B] [Class: 1B] [Service: 1B] [Dest: 1B] [Src: 1B] [Payload: N B] [CRC16: 2B]
```

---

## 🚀 Projeye Dahil Etme (Entegrasyon)

### 1. CMake Yapılandırması (`CMakeLists.txt`)

Uygulamanızın `CMakeLists.txt` dosyasına `deos_core` ve kullanacağınız transport kütüphanelerini ekleyin:

```cmake
# Kütüphane yollarını projeye dahil edin
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/deos_core build_deos_core)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/deos_transport_can build_deos_can)

# Hedefinize bağlayın
target_link_libraries(app PRIVATE 
    deos_core 
    deos_transport_can
)
```

### 2. Zephyr Kconfig Yapılandırması (`prj.conf`)

Uygulamanızın `prj.conf` dosyasında gerekli RTOS özelliklerini açın:

```ini
# C++ Desteği (Uygulamanız C++ ile yazılıyorsa)
CONFIG_CPP=y
CONFIG_STD_CPP17=y

# CAN ve CAN-FD Desteği
CONFIG_CAN=y
CONFIG_CAN_FD_MODE=y
CONFIG_CAN_MAX_FILTER=10

# Thread ve Zamanlayıcılar
CONFIG_MULTITHREADING=y

# Zephyr Logging (Geliştirme aşamasında INF/DBG, prodüksiyonda kapatılabilir)
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

---

## 💻 Kullanım Kılavuzu: Uç Düğüm (Endpoint Node) Oluşturma

Bir sensör, aktüatör veya motor sürücü kartı (örneğin Direksiyon Düğümü `DEOS_NODE_STEERING`) geliştirirken izlenecek adımlar:

### Adım 1: Gerekli Başlıkları Ekleyin
```c
#include <deos/deos.h>
#include <deos/deos_transport_can.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(steering_app, LOG_LEVEL_INF);
```

### Adım 2: Gelen Komutlar İçin Callback (Handler) Tanımlayın
```c
static int handle_target_angle(const deos_message_t *msg, void *user_data)
{
    if (msg->payload_len < sizeof(int16_t)) {
        LOG_WRN("Eksik veri boyutu!");
        return -EINVAL;
    }

    int16_t target_angle;
    memcpy(&target_angle, msg->payload, sizeof(target_angle));
    LOG_INF("Yeni aci hedefi alindi: %d derece", target_angle);

    // Yanit yolla (Opsiyonel)
    deos_send_response(msg, DEOS_RESULT_OK, NULL, 0);
    return 0;
}
```

### Adım 3: Düğümü İlklendirin ve Başlatın
```c
int main(void)
{
    int ret;

    // 1. Core Konfigürasyonu
    struct deos_config cfg = {
        .node_id = DEOS_NODE_STEERING,
        .router_enabled = false,          // Bu kart bir Router degil, Endpoint!
        .hosted_nodes_enabled = false     // Tek bir mantiksal dugum var
    };

    ret = deos_init(&cfg);
    if (ret != 0) {
        LOG_ERR("DEOS Core ilklendirilemedi: %d", ret);
        return ret;
    }

    // 2. CAN-FD Transport Ilklendirmesi (Devicetree birincil CAN'i otomatik alir)
    ret = deos_can_transport_init(NULL);
    if (ret != 0) {
        LOG_ERR("CAN transport baslatilamadi: %d", ret);
        return ret;
    }

    // 3. Komut Dinleyiciyi (Handler) Kaydedin
    deos_register_handler(
        DEOS_CLASS_COMMAND,
        DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_SET_TARGET_ANGLE,
        handle_target_angle,
        NULL
    );

    // 4. DEOS Protokol Operasyonunu Baslatin
    deos_start();
    LOG_INF("Direksiyon dugumu calisiyor.");

    // 5. Periyodik Durum / Telemetri Gönderimi (Ana Döngü)
    while (1) {
        k_msleep(100);

        int16_t current_angle = 120; // Ornek sensor verisi
        deos_send(
            DEOS_NODE_MAIN_STM32,             // Hedef
            DEOS_PRIO_STATUS,                 // Oncelik
            DEOS_CLASS_STATUS,                // Sinif
            DEOS_SERVICE_STEERING,            // Servis
            DEOS_CMD_STEERING_GET_STATUS,     // Komut
            &current_angle,
            sizeof(current_angle)
        );
    }

    return 0;
}
```

---

## 🔀 Kullanım Kılavuzu: Çoklu Mantıksal Düğüm (Hosted Nodes)

Tek bir fiziksel mikrodenetleyici üzerinde birden fazla DEOS mantıksal düğümü koşabilir (örneğin tek kartın hem Sol Motor hem Sağ Motor sürücüsünü kontrol etmesi gibi).

```c
// 1. Konfigürasyonda hosted_nodes_enabled = true yapin
struct deos_config cfg = {
    .node_id = DEOS_NODE_TRACTION_LEFT,  // Birincil dugum
    .router_enabled = false,
    .hosted_nodes_enabled = true         // Coklu dugum destegi acik!
};
deos_init(&cfg);

// 2. Ikinci mantiksal dugumu sisteme tanitin
deos_register_local_node(DEOS_NODE_TRACTION_RIGHT);

// 3. Her dugum icin ayri ayri handler baglayin
deos_register_handler_for_node(
    DEOS_NODE_TRACTION_LEFT,
    DEOS_CLASS_COMMAND,
    DEOS_SERVICE_TRACTION,
    DEOS_CMD_TRACTION_SET_SPEED,
    left_motor_handler,
    NULL
);

deos_register_handler_for_node(
    DEOS_NODE_TRACTION_RIGHT,
    DEOS_CLASS_COMMAND,
    DEOS_SERVICE_TRACTION,
    DEOS_CMD_TRACTION_SET_SPEED,
    right_motor_handler,
    NULL
);

// 4. Spesifik bir yerel dugum adina paket gonderin
int16_t right_rpm = 2500;
deos_send_from_node(
    DEOS_NODE_TRACTION_RIGHT,         // Kaynak: Sag motor
    DEOS_NODE_MAIN_STM32,             // Hedef: Ana kontrolcu
    DEOS_PRIO_STATUS,
    DEOS_CLASS_STATUS,
    DEOS_SERVICE_TRACTION,
    DEOS_CMD_TRACTION_GET_STATUS,
    &right_rpm,
    sizeof(right_rpm)
);
```

> [!NOTE]
> Ağ üzerinden `GLOBAL_BROADCAST` (Acil Durum / E-Stop) geldiğinde, DEOS Core döngüsü her iki düğümün (`LEFT` ve `RIGHT`) handler'ını sırayla tetikler. Hiçbir düğüm yayından mahrum kalmaz.

---

## 🌐 Kullanım Kılavuzu: Yönlendirici / Ağ Geçidi (Router Node) Oluşturma

Araçtaki Ana Bilgisayar / Ağ Geçidi (örneğin `MainApp` / STM32H5), CAN-FD, LoRa ve Ethernet arasında paket yönlendirir:

```cpp
// 1. Rota Belirleme Fonksiyonu (Routing Table Policy)
deos_transport_t my_routing_policy(deos_node_id_t destination)
{
    switch (destination) {
        case DEOS_NODE_GROUND_CONTROL:
        case DEOS_NODE_DIAG_TOOL:
            return DEOS_TRANSPORT_UART_LORA;     // Telsiz hattina yonlendir

        case DEOS_NODE_AUTONOMOUS_PC:
            return DEOS_TRANSPORT_ETH_ROS_BRIDGE;// Ethernet ROS hattina yonlendir

        case DEOS_NODE_STEERING:
        case DEOS_NODE_TRACTION:
        case DEOS_NODE_BRAKE:
        case DEOS_NODE_BMS:
            return DEOS_TRANSPORT_CAN_FD;        // Alt sistemlere CAN-FD ile ulas

        default:
            return DEOS_TRANSPORT_UNKNOWN;       // Tanimsizsa Router paketi atar ve Fault uretir
    }
}

void init_router(void)
{
    // 2. Router modunda deos_init
    struct deos_config cfg = {
        .node_id = DEOS_NODE_MAIN_STM32,
        .router_enabled = true,
        .hosted_nodes_enabled = true
    };
    deos_init(&cfg);

    // 3. Rota tablosunu baglayin
    deos_router_set_lookup_fn(my_routing_policy);

    // 4. CAN-FD Transportunu baslatin
    deos_can_transport_init(NULL);

    // 5. Diger transportlari (LoRa, Ethernet) TX callback'i ile baglayin
    deos_router_register_transport(DEOS_TRANSPORT_UART_LORA, lora_tx_callback);
    deos_router_register_transport(DEOS_TRANSPORT_ETH_ROS_BRIDGE, eth_tx_callback);

    // 6. Router'i baslatin
    deos_start();
}

// 7. LoRa veya Ethernet'ten ham byte geldiginde sisteme besleme:
void on_lora_bytes_received(uint8_t *buffer, size_t len)
{
    // Ham veriyi CRC kontroluyle cozer, gecersizse fault uretir, saglamsa yonlendirir
    deos_feed_frame(buffer, len, DEOS_TRANSPORT_UART_LORA);
}
```

---

## 📡 Telemetri & Abonelik Sistemi (Pub/Sub)

DEOS, bant genişliğini korumak için akıllı bir abonelik motoruna sahiptir:

```c
// Bir dugumun durumuna 100 ms aralikla abone ol (10 saniye boyunca gecerli lease)
deos_subscribe(
    DEOS_NODE_BMS,
    DEOS_SERVICE_BMS,
    DEOS_CMD_BMS_GET_BATTERY_STATUS,
    100,    // 100 ms aralik
    10      // 10 saniye TTL
);

// Abonelikten cik
deos_unsubscribe(
    DEOS_NODE_BMS,
    DEOS_SERVICE_BMS,
    DEOS_CMD_BMS_GET_BATTERY_STATUS
);

// Uretici Dugum tarafinda: Durum degistiginde (On-Change) aboneleri uyar
deos_subscription_notify_change(DEOS_SERVICE_BRAKE, DEOS_CMD_BRAKE_GET_STATUS);
```

---

## ⚠️ Düğüm Bazlı Hata Yönetimi (Fault Management)

Uygulamanızda kritik bir donanım veya mantık hatası oluştuğunda `deos_fault.h` API'si kullanılır:

```c
#include <deos/deos_fault.h>

// Bir ariza olustugunda:
if (sensor_read_failed) {
    deos_fault_raise(DEOS_FAULT_CAN_BUS_OFF, DEOS_FAULT_SEVERITY_ERROR);
}

// Belirli bir yerel dugum adina hata bildirmek icin:
deos_fault_raise_for_node(
    DEOS_NODE_TRACTION_LEFT, 
    DEOS_FAULT_ROUTER_UNMAPPED_ROUTE, 
    DEOS_FAULT_SEVERITY_WARNING
);
```

Hatalar düğümün dahili mandallama (latch) tablosuna kaydedilir. Yer istasyonundan veya teşhis aracından `GET_FAULTS` komutu geldiğinde otomatik olarak sırayla raporlanır.

---

## 🧩 C++ Nesne Yönelimli Entegrasyon Örneği

C++ sınıfları içinden DEOS mesajlarını dinlemek için statik bir callback köprüsü kullanılır:

```cpp
class TractionController {
public:
    void init() {
        // 'this' isaretcisini user_data olarak kaydederiz
        deos_register_handler(
            DEOS_CLASS_COMMAND,
            DEOS_SERVICE_TRACTION,
            DEOS_CMD_TRACTION_SET_SPEED,
            &TractionController::static_speed_handler,
            this
        );
    }

private:
    static int static_speed_handler(const deos_message_t *msg, void *user_data) {
        auto *self = static_cast<TractionController *>(user_data);
        return self->handle_speed(msg);
    }

    int handle_speed(const deos_message_t *msg) {
        // C++ sinif fonksiyonu burada dogrudan nesne degiskenlerine erisebilir
        LOG_INF("Motor hizi uygulandi.");
        return 0;
    }
};
```

---

## 📌 Sık Yapılan Hatalar ve Önemli Kurallar

1. **Include Yolları:** Asla `../../lib/deos_core/...` gibi göreli yollar kullanmayın. Her zaman `<deos/deos.h>` veya `<deos/deos_icd.h>` şeklinde sistem include formatını kullanın.
2. **`printf` Kullanmayın:** Zephyr'in yerel `<zephyr/logging/log.h>` altyapısını (`LOG_INF`, `LOG_ERR`, `LOG_WRN`, `LOG_DBG`) kullanın.
3. **Core Kütüphaneyi Değiştirmeyin:** `lib/deos_core` protokol kurallarını belirler. İhtiyaç duyulan değişiklikler Emir Arkalı onayına tabidir.
4. **CAN-FD Ayırma Kuralı:** CAN donanım işlemleri `lib/deos_transport_can` altındadır. Uygulama kodları içinde CAN filtrelerini manuel yapılandırmak yerine `deos_can_transport_init(NULL)` çağrısını kullanın.
5. **Dinamik Bellek:** `malloc`, `new`, `std::vector` (dinamik boyutlu) gibi gömülü sistem determinizmini bozan yapılardan kaçının.

---

## 📄 Lisans & Sahiplik
Bu yazılım **DEUDEOS Otonom Araç Ekibi** bünyesinde geliştirilmiştir.  
Geliştirici & Mimari: [Emir Arkalı](https://github.com/emirarkali)  
Dokümantasyon & ICD Deposu: [DEUDEOS-Team/deos-docs](https://github.com/DEUDEOS-Team/deos-docs)

# DEOS Core Protocol Stack

**DEOS Core**, Zephyr RTOS üzerinde koşan dağıtık STM32 düğümleri (nodes) için özel olarak geliştirilmiş hafif (lightweight), deterministik ve CAN-FD tabanlı araç içi haberleşme protokol yığınıdır.

Bu depo, kütüphanenin bağımsız (**standalone**) saf kütüphane ve **Zephyr Modülü** versiyonudur. İstenilen herhangi bir Zephyr projesine kolayca dahil edilebilir.

---

## 🚀 Projeye Entegrasyon Yöntemleri

### Yöntem 1: Git Submodule (En Çok Tercih Edilen)
Projenizin kök dizininde veya kütüphane klasöründe:

```bash
git submodule add https://github.com/emirarkali/deos-core-protocol-stack.git lib/deos_core
```

Uygulamanızın `CMakeLists.txt` dosyasına ekleyin:
```cmake
add_subdirectory(lib/deos_core)
target_link_libraries(app PRIVATE deos_core)
```

`prj.conf` dosyanıza CAN ve CAN-FD desteğini açın:
```ini
CONFIG_CAN=y
CONFIG_CAN_FD_MODE=y
```

---

### Yöntem 2: Zephyr Modülü Olarak (West Manifest)
`west.yml` dosyanızın `projects` listesine ekleyin:

```yaml
manifest:
  projects:
    - name: deos-core
      url: https://github.com/emirarkali/deos-core-protocol-stack.git
      revision: main
      path: modules/lib/deos_core
```

Zephyr `module.yml` tanımlı olduğu için derleme sistemi kütüphaneyi ve Kconfig ayarlarını otomatik olarak tanıyacaktır.

---

### Yöntem 3: Git Subtree ile Ekleme
```bash
git subtree add --prefix=lib/deos_core https://github.com/emirarkali/deos-core-protocol-stack.git main --squash
```

---

## 🏗 Temel Özellikler (Core Features)

- **Sıfır Dinamik Bellek (Zero Dynamic Memory):** Heap veya `malloc` kullanılmaz. Tüm kuyruklar, buffer'lar ve context'ler static / BSS üzerinde tahsis edilir.
- **Hosted Local Nodes:** Tek bir fiziksel MCU birden fazla mantıksal düğümü (Node) barındırabilir. Her düğümün `Sequence Counter`, `Fault Table` ve `Telemetry Context`'i birbirinden bağımsızdır.
- **Donanımsal RX Filtreleme (Hardware RX Filters):** Yalnızca kayıtlı düğümlere ve Broadcast adresine gelen mesajların MCU'yu uyandırmasını sağlayan donanım destekli CAN filtreleri.
- **Otonom PING/PONG:** Ağdaki canlılığı kontrol etmek için uygulamanın haberi olmadan arka planda (doğru source ID ile) otomatik PONG yanıtı üretir.
- **Subscription Engine (Telemetri Abonelik Motoru):** Periyodik (10ms, 100ms, 1000ms) veya Olay Tabanlı (On-Change) telemetri aboneliklerini TTL ve hata yönetimi ile otonom yönetir.
- **Düğüm Bazlı Hata Yönetimi (Fault Management):** Hatalar düğüm bazında kaydedilir, mandallanır (latch) ve `GET_FAULTS` komutu geldiğinde `k_work_delayable` ile non-blocking stream edilir.

---

## 📡 29-bit CAN ID Formatı (Extended Identifier)

| Bit Range | Uzunluk | Alan | Açıklama |
| :--- | :--- | :--- | :--- |
| **28..26** | 3 bit | **Priority** | Mesaj önceliği (`URGENT=0x01`, `COMMAND=0x02`, `STATUS=0x04`) |
| **25..22** | 4 bit | **Message Class** | Mesaj sınıfı (`COMMAND=0x01`, `STATUS=0x02`, `EVENT=0x03`, `DIAGNOSTIC=0x06`) |
| **21..16** | 6 bit | **Service ID** | İlgili servis (`SYSTEM=0x00`, `TRACTION=0x01`, `STEERING=0x02`, `BRAKE=0x03` vb.) |
| **15..8** | 8 bit | **Destination Node** | Hedef Düğüm ID (`0xFF`: Global Broadcast, `0xFE`: CAN Broadcast) |
| **7..0** | 8 bit | **Source Node** | Kaynak Düğüm ID |

---

## 📦 Veri Başlığı (Common Data Header - 4 Bytes)

| Offset | Alan | Açıklama |
| :---: | :--- | :--- |
| **0** | `version` | Protokol Sürümü (`0x11` = v1.1) |
| **1** | `sequence`| Mesaj sıra sayacı (Her kaynak node için bağımsız artar) |
| **2** | `command` | Komut/Telemetri ID |
| **3** | `length`  | Payload byte uzunluğu (Header hariç, maks 60 byte) |
| **4..N**| `payload` | Little-Endian sıralı veri alanı |

---

## 💻 Hızlı Başlangıç (Quick Start Code)

```c
#include <deos/deos.h>

void main(void) {
    /* 1. Core konfigürasyonu */
    struct deos_config config = {
        .node_id = DEOS_NODE_STEERING,
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus)),
        .router_enabled = false,
        .hosted_nodes_enabled = true
    };

    /* 2. Kütüphaneyi ilklendir */
    if (deos_init(&config) != 0) {
        LOG_ERR("DEOS Init failed!");
        return;
    }

    /* 3. Barındırılan ek Local Node kayıt et (Örn: Brake) */
    deos_register_local_node(DEOS_NODE_BRAKE);

    /* 4. Gelen komutlar için Handler bağla */
    deos_register_handler_for_node(
        DEOS_NODE_STEERING,
        DEOS_CLASS_COMMAND,
        DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_SET_TARGET_ANGLE,
        steering_cmd_handler,
        NULL
    );

    /* 5. RX thread ve donanım filtrelerini aktif et */
    deos_start();

    /* 6. Mesaj Gönderimi */
    uint16_t current_angle = 450;
    deos_send_from_node(
        DEOS_NODE_STEERING,
        DEOS_NODE_MAIN_STM32,
        DEOS_PRIO_STATUS,
        DEOS_CLASS_STATUS,
        DEOS_SERVICE_STEERING,
        DEOS_CMD_STEERING_GET_STATUS,
        &current_angle,
        sizeof(current_angle)
    );
}
```

---

## 📄 Lisans & Sahiplik
Bu yazılım **DEUDEOS Otonom Araç Ekibi** bünyesinde geliştirilmiştir.
Geliştirici: [Emir Arkalı](https://github.com/emirarkali)
Dokümantasyon & ICD Deposu: [DEUDEOS-Team/deos-docs](https://github.com/DEUDEOS-Team/deos-docs)

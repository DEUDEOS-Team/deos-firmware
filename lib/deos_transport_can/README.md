# DEOS CAN-FD Transport Eklentisi (`deos_transport_can`)

Bu kütüphane, **DEOS Core** protokol yığını için donanım seviyesinde **CAN-FD** iletim ve alım (TX/RX) sürücüsüdür. Zephyr RTOS'un `drivers/can.h` altyapısını kullanarak `deos_message_t` mantıksal mesajları ile 29-bit Extended CAN-FD çerçeveleri arasındaki dönüşümü ve donanımsal filtrelemeyi yönetir.

---

## 📡 29-bit CAN ID Formatı (Extended Identifier)

CAN-FD hattı üzerinden taşınan her paketin 29-bit Extended CAN tanımlayıcısı (Identifier) şu şekilde kodlanır ve çözümlenir (`deos_can_codec.c`):

```
 28   26 25   22 21      16 15            8 7             0
+-------+-------+----------+---------------+---------------+
| PRIO  | CLASS | SERVICE  |  DESTINATION  |    SOURCE     |
| 3 bit | 4 bit |  6 bit   |     8 bit     |     8 bit     |
+-------+-------+----------+---------------+---------------+
```

| Bit Aralığı | Uzunluk | Alan Adı | Açıklama | Değer Örnekleri |
| :--- | :---: | :--- | :--- | :--- |
| **28..26** | 3 bit | **Priority** | Mesaj önceliği | `EMERGENCY=0x00`, `SAFETY=0x01`, `COMMAND=0x02`, `STATUS=0x04` |
| **25..22** | 4 bit | **Message Class** | Mesaj sınıfı | `COMMAND=0x01`, `STATUS=0x02`, `EVENT=0x03`, `REQUEST=0x04`, `RESPONSE=0x05`, `DIAGNOSTIC=0x06`, `NETWORK=0x07` |
| **21..16** | 6 bit | **Service ID** | Hedef Servis | `SYSTEM=0x00`, `TRACTION=0x01`, `STEERING=0x02`, `BRAKE=0x03`, `BMS=0x04` vb. |
| **15..8**  | 8 bit | **Destination Node** | Hedef Düğüm ID | `0x01`: MAIN_STM32, `0x02`: STEERING, `0xFE`: CAN Broadcast, `0xFF`: Global Broadcast |
| **7..0**   | 8 bit | **Source Node** | Gönderen Düğüm | Mesajı üreten kaynak düğüm ID'si |

---

## 📦 CAN-FD Veri Alanı Başlığı (Common Header - 4 Bayt)

CAN-FD veri yükünün (Data Payload - 64 Bayt) ilk 4 baytı ortak başlık olarak kullanılır:

| Offset | Alan Adı | Açıklama |
| :---: | :--- | :--- |
| **0** | `version` | Protokol sürümü (`0x11` = v1.1) |
| **1** | `sequence` | Kaynak düğümün bağımsız sıra sayacı |
| **2** | `command` | Servise ait komut/telemetri ID'si |
| **3** | `length` | Net veri uzunluğu (Maks. 60 bayt) |
| **4..63** | `payload` | Little-Endian sıralı komut verisi |

---

## 🚀 Projeye Entegrasyon ve Kullanım

### 1. `CMakeLists.txt`
```cmake
add_subdirectory(lib/deos_transport_can)
target_link_libraries(app PRIVATE deos_transport_can)
```

### 2. `prj.conf`
```ini
CONFIG_CAN=y
CONFIG_CAN_FD_MODE=y
CONFIG_CAN_MAX_FILTER=10
```

### 3. Kullanım (C / C++)
Uygulama katmanında sadece tek bir satır ile başlatılır. Devicetree üzerindeki birincil CAN-FD denetleyicisini otomatik bulur ve arka plan RX/TX thread'lerini ayağa kaldırır:

```c
#include <deos/deos_transport_can.h>

int ret = deos_can_transport_init(NULL);
if (ret != 0) {
    LOG_ERR("CAN-FD Transport baslatilamadi: %d", ret);
}
```

---

## 🛡️ Donanımsal Filtreleme (Hardware RX Filtering)

`deos_can_transport_init` çalıştırıldığında, MCU'nun CAN donanımına (bxCAN / FDCAN) iki adet donanımsal filtre kurulur:
1. **Yerel Düğüme Özel Filtre:** Yalnızca hedefi bu mikrodenetleyicinin `node_id`'si olan paketler CPU'yu uyandırır.
2. **Genel Yayın (Broadcast) Filtresi:** `0xFF` (Global) ve `0xFE` (CAN Broadcast) paketlerini dinler.

Diğer düğümlerin aralarındaki rutin CAN-FD trafiği donanım seviyesinde elenir; CPU çekirdeğine kesme (IRQ) üretmez.

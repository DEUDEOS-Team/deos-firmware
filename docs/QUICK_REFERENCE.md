# DEOS Protokolü - Hızlı Mühendislik Referans Kartı (Quick Reference)

Bu doküman; **Firmware (STM32)**, **Otonom Sürüş (ROS 2)**, **Yer Kontrol İstasyonu (Python)** ve **Sensör/BMS Entegrasyonu** geliştiren mühendislerin günlük çalışmada aynı dili konuşması, hızlıca değerleri teyit etmesi ve entegrasyon hatalarını önlemesi için hazırlanmış tek sayfalık pratik referans özetidir.

* **Protokol Wire Sürümü:** `v1.1` (`0x11`)
* **Doküman Seti:** `DOC-ICD-CORE-001` - `DOC-ICD-NODE-BMS-012`
* **Bayt Dizilimi (Endianness):** **TÜM VERİLER LITTLE-ENDIAN (LSB İLK).**

---

## 1. Düğüm Kimlikleri (Node ID Registry)

| Node ID | Sembol | Düğüm Türü | Çalıştığı Donanım / Hat | Barındırdığı Birincil Servisler |
|:---:|---|---|---|---|
| `0x00` | `DEOS_NODE_INVALID` | Tanımsız | - | Geçersiz / Hatalı Adres |
| `0x01` | `DEOS_NODE_TEXTUAL` | Sanal / Arayüz | RPi / Operatör PC (Ethernet TCP) | SYSTEM (`0x00`), CONFIG (`0x06`) |
| `0x02` | `DEOS_NODE_GROUND_CONTROL` | Sanal / Uzak | Yer İstasyonu PC (LoRa UART) | SYSTEM (`0x00`), Telemetri |
| `0x03` | `DEOS_NODE_MAIN_STM32` | Fiziksel Cihaz | STM32H563ZI (Araç Ana Beyni) | SYSTEM (`0x00`), LIGHTING (`0x08`), Router |
| `0x04` | `DEOS_NODE_ROS_BRIDGE` | Sanal / Köprü | RPi / Jetson / PC (Ethernet UDP) | SYSTEM (`0x00`), Otonom Sürüş Telemetri & Komut |
| `0x10` | `DEOS_NODE_TRACTION` | Fiziksel ECU | STM32 Traction ECU (CAN-FD) | TRACTION (`0x01`), SYSTEM (`0x00`) |
| `0x11` | `DEOS_NODE_STEERING` | Fiziksel ECU | STM32 Steering ECU (CAN-FD) | STEERING (`0x02`), SYSTEM (`0x00`) |
| `0x12` | `DEOS_NODE_BRAKE` | Fiziksel ECU | STM32 Brake ECU (CAN-FD) | BRAKE (`0x03`), SYSTEM (`0x00`) |
| `0x13` | `DEOS_NODE_IMU` | Sanal / Hosted | STM32 Main Node Üzerinde Lokal | SENSORS (`0x07`), SYSTEM (`0x00`) |
| `0x20` | `DEOS_NODE_BMS_MAIN` | Sanal / Hosted | BMS Gateway Üzerinde Lokal | BMS (`0x04`), SYSTEM (`0x00`) |
| `0x21` | `DEOS_NODE_BMS_AUX` | Sanal / Hosted | BMS Gateway Üzerinde Lokal | BMS (`0x04`), SYSTEM (`0x00`) |
| `0x30` | `DEOS_NODE_BMS_GATEWAY` | Fiziksel Cihaz | CAN Gateway Donanımı (CAN-FD) | SYSTEM (`0x00`), Classic CAN Köprüsü |
| `0xF0` | `DEOS_NODE_DIAG_TOOL` | Sanal / Test | Harici Teşhis / Test Aracı | SYSTEM (`0x00`), DIAGNOSTIC (`0x06`) |
| `0xFE` | `DEOS_NODE_CAN_BROADCAST` | Mantıksal Yayın | Yalnızca CAN-FD Omurgası | Yalnızca CAN Hattına Yayın (Heartbeat vb.) |
| `0xFF` | `DEOS_NODE_GLOBAL_BROADCAST`| Mantıksal Yayın | Tüm Ağ (CAN, LoRa, Ethernet) | Tüm Sisteme Genel Yayın (E-STOP, State Değişimi) |

---

## 2. Ortak Servis Kimlikleri (Service IDs)

| Service ID | Sembol | Fonksiyonel Alan |
|:---:|---|---|
| `0x00` | `DEOS_SERVICE_SYSTEM` | PING, PONG, HEARTBEAT, SET_STATE, GET_STATE, REBOOT |
| `0x01` | `DEOS_SERVICE_TRACTION` | Hız hedefi, tork tavanı, motor devri, invertör durumu |
| `0x02` | `DEOS_SERVICE_STEERING` | Direksiyon açısı hedef/mevcut, kalibrasyon, motor akımı |
| `0x03` | `DEOS_SERVICE_BRAKE` | Fren basıncı, acil fren (EBS), aktüatör pozisyonu |
| `0x04` | `DEOS_SERVICE_BMS` | Hücre voltajları, akım, sıcaklık, SOC/SOH, kontaktör |
| `0x05` | `DEOS_SERVICE_CALIBRATION` | Sensör sıfırlama, potansiyometre kalibrasyonu |
| `0x06` | `DEOS_SERVICE_DIAGNOSTIC` | DTC Arıza kayıtları, hata temizleme |
| `0x07` | `DEOS_SERVICE_SENSORS` | IMU ivme/jiro, ultrasonik, süspansiyon sensörleri |
| `0x08` | `DEOS_SERVICE_LIGHTING` | Farlar, sinyaller, fren lambası (Hedef: `DEOS_NODE_MAIN_STM32`) |

---

## 3. Taşıma Ortamları, Portlar ve Parametreler

```
[ROS 2 Otonom / Jetson]      [Textual CLI / Python]       [Yer İstasyonu / GUI]
        │ UDP 5001                    │ TCP 5000                   │ LoRa (115200 Baud)
        ▼                             ▼                            ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          DEOS_NODE_MAIN_STM32 (0x03)                        │
│                           Merkezi Ağ Yönlendirici                           │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ CAN-FD (1M / 5M BRS)
                 ┌─────────────────────┼─────────────────────┐
                 ▼                     ▼                     ▼
          [Traction 0x10]       [Steering 0x11]        [Brake 0x12]
```

| Ortam | Protokol / Fiziksel | Port / Parametre | Çerçeveleme (Framing) Kuralı |
|---|---|---|---|
| **ROS 2 Köprüsü** | Ethernet UDP (`SOCK_DGRAM`) | `192.168.1.10:5001` | `[1B Length] [8B Header] [N Bayt Payload]`<br>*(Dinamik Client Discovery; ek CRC yok)* |
| **Textual Konsol** | Ethernet TCP (`SOCK_STREAM`) | `192.168.1.10:5000` | `[1B Length] [8B Header] [N Bayt Payload]`<br>*(TCP akış kontrolü; ek CRC yok)* |
| **Yer Kontrolü** | LoRa / UART Seri Port | 115200 Baud, Kanal 23, 2.4 kbps | `[0xAA 0x55] [Flag] [Length] [Payload] [CRC16]`<br>*(Donanımsal kesme & semafor, Zero-Latency)* |
| **Araç İçi Omurga** | CAN-FD Fiziksel Katman | 1 Mbps Nominal / 5 Mbps Data (BRS) | 29-bit Extended CAN ID + DLC Padding (Little-Endian) |

---

## 4. Standart DEOS Paket Anatomisi

### 8 Baytlık Ortak Başlık (Common Header)
Tüm medya katmanlarında (`CAN-FD`, `TCP`, `UDP`, `LoRa`) taşınan standart çekirdek başlıktır:

```
 Byte 0      Byte 1      Byte 2      Byte 3      Byte 4      Byte 5      Byte 6      Byte 7
┌───────────┬───────────┬───────────┬───────────┬───────────┬───────────┬───────────┬───────────┐
│ Priority  │ Class     │ Service   │ Dest Node │ Src Node  │ Version   │ Sequence  │ Command   │
│ (1 Byte)  │ (1 Byte)  │ (1 Byte)  │ (1 Byte)  │ (1 Byte)  │ (0x11)    │ (0-255)   │ (1 Byte)  │
└───────────┴───────────┴───────────┴───────────┴───────────┴───────────┴───────────┴───────────┘
```

* **Öncelik Seviyeleri (Priority):**
  * `0x00`: `DEOS_PRIO_EMERGENCY` (E-STOP, acil güvenlik)
  * `0x01`: `DEOS_PRIO_CONTROL` (Direksiyon açısı, gaz/fren talebi)
  * `0x02`: `DEOS_PRIO_STATUS` (Geri bildirim, sensör telemetrisi)
  * `0x03`: `DEOS_PRIO_CONFIG` (Parametre ayarları, debug)
* **Mesaj Sınıfları (Class):**
  * `0x00`: `COMMAND` (Yapılacak eylem talebi)
  * `0x01`: `REQUEST` (Bilgi isteme - Get)
  * `0x02`: `RESPONSE` (Cevap paketi)
  * `0x03`: `STATUS` (Periyodik yayın)
  * `0x04`: `FAULT` (Hata bildirimi)

---

## 5. Mühendislik Birimleri ve Standart Ölçekleme (Units & Scaling)

Takımlar arasında birim karmaşasını (örn: radyan mı derece mi, m/s mi km/h mı) önlemek için **tüm sistemde uygulanan kesin kurallar**:

| Sinyal Adı | Veri Tipi | Birim | Çarpan / Ölçek | Örnek Ham Değer | Gerçek Fiziksel Değer |
|---|---|:---:|:---:|:---:|:---:|
| **Hedef / Mevcut Hız** | `int32_t` | m/s | `0.01` ($10^{-2}$) | `500` | $5.00\text{ m/s}$ ($18\text{ km/h}$) |
| **Hedef / Mevcut İvme** | `int16_t` | m/s² | `0.01` ($10^{-2}$) | `150` | $1.50\text{ m/s}^2$ |
| **Direksiyon Açısı** | `int16_t` | Derece (°) | `0.01` ($10^{-2}$) | `+1500` / `-1500` | $+15.00^\circ$ (Sol) / $-15.00^\circ$ (Sağ) |
| **Tork / Gaz Talebi** | `uint16_t` | % | `0.01` ($10^{-2}$) | `10000` | $100.00\%$ |
| **Fren Basıncı** | `uint16_t` | Bar | `0.1` ($10^{-1}$) | `250` | $25.0\text{ bar}$ |
| **Batarya Paketi Voltajı** | `uint16_t` | Volt (V) | `0.01` ($10^{-2}$) | `5420` | $54.20\text{ V}$ |
| **Hücre Voltajı** | `uint16_t` | mV | `1.0` | `3650` | $3.650\text{ V}$ |
| **Batarya Akımı** | `int16_t` | Amper (A) | `0.1` ($10^{-1}$) | `+350` / `-120` | $+35.0\text{ A}$ (Şarj) / $-12.0\text{ A}$ (Deşarj) |
| **Sıcaklık** | `int8_t` | °C | `1.0` | `38` | $38^\circ\text{C}$ |
| **IMU İvme ($X, Y, Z$)** | `int16_t` | m/s² | `0.01` ($10^{-2}$) | `981` | $9.81\text{ m/s}^2$ |
| **IMU Açısal Hız ($X, Y, Z$)** | `int16_t` | °/s | `0.01` ($10^{-2}$) | `250` | $2.50^\circ\text{/s}$ |

---

## 6. Kritik Sistem & Mimari Kuralları (Golden Rules)

1. **Split Horizon Kuralı (Loopback Koruması):**
   * STM32 Router, bir arayüzden (örn. LoRa) aldığı paketi **asla aynı arayüze geri basmaz**.
   * *Test Tuzağı:* Yer İstasyonundan (0x02) ağ testi yaparken hedef adrese `0x02` yazarsanız router paketi çöpe atar! Test için her zaman hedef adrese **`DEOS_NODE_MAIN_STM32` (`0x03`)** yazıp PING atınız; STM32 size PONG cevabı üretir.
2. **Broadcast Kullanımı:**
   * **`0xFE` (`DEOS_NODE_CAN_BROADCAST`):** Saniyede 1 kez atılan `HEARTBEAT` gibi yüksek frekanslı rutin mesajlar için kullanılır. LoRa hattını tıkamamak için bu yayın **yalnızca donanımsal CAN-FD hattında** kalır.
   * **`0xFF` (`DEOS_NODE_GLOBAL_BROADCAST`):** `E-STOP` ve Sistem Durumu (`SET_STATE`) gibi tüm araçların ve yer kontrolün anında duyması gereken kritik olaylar için kullanılır (CAN, LoRa, Ethernet hepsine basılır).
3. **Aydınlatma (Lighting) Servis Rolü:**
   * Araçta bağımsız bir "Lighting ECU" yoktur.
   * Aydınlatmayı açıp kapatmak için `DEOS_NODE_MAIN_STM32` (`0x03`) hedef düğümüne `DEOS_SERVICE_LIGHTING` (`0x08`) servis komutu yollanır. STM32 üzerindeki transistör sürücüler farları/röleleri yakar.
4. **Bellek Boyutu vs Hat Boyutu (Airtime Tasarrufu):**
   * RAM'de azami struct boyutu 84 bayttır (`sizeof(deos_message_t)`).
   * Ancak hat üzerinde (özellikle LoRa ve UDP/TCP) asla boş baytlar basılmaz; daima dinamik olarak **`8 + payload_len`** bayt aktarılır.

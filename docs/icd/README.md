# DEOS Arayüz Kontrol Dokümanları (ICD) Kılavuzu & İndeksi

Bu dizin, **DEOS (Dokuz Eylül Otonom Sistemler)** araç içi ve araç dışı dağıtık haberleşme ekosistemini tanımlayan resmi **Arayüz Kontrol Dokümanı (Interface Control Document - ICD)** şartnamesini içerir.

Protokolün monolitik ilk sürümü arşivlenmiş (`docs/DEOS_CANFD_ICD_v0_3.tex`), tüm sistem **medya bağımsız çekirdek**, **taşıma katmanları**, **sistem servisleri** ve **alt sistem düğümleri** olmak üzere 12 bağımsız modüler dokümana ayrılmıştır.

> **Hızlı Referans Kartı (Cheat Sheet):** Tüm Node ID, Service ID, portlar ve mühendislik birim ölçekleme katsayılarının tek sayfalık pratik mühendislik özeti için [`docs/QUICK_REFERENCE.md`](../QUICK_REFERENCE.md) dokümanını inceleyebilirsiniz.

---

## 1. Genel Metaveri & Versiyonlama

| Alan | Bilgi |
|---|---|
| **Protokol Wire Sürümü** | **v1.1** (`0x11`) |
| **Doküman Ailesi Sürümü** | **Sürüm 1.1** |
| **Doküman Sahibi & Yazar** | Emir Arkalı (`emirarkali07@gmail.com`) |
| **İlk Yayın Tarihi** | 28 Eylül 2026 |
| **Son Güncelleme** | 13 Ekim 2026 |
| **Durum** | Onaylandı / Aktif Normatif Şartname |

---

## 2. Doküman Listesi ve Hiyerarşik Okuma Sırası

Yeni geliştiricilerin ve sistem entegratörlerinin şartnameyi aşağıdaki sırayla incelemesi tavsiye edilir:

```
[Seviye 1: Çekirdek & Tipler]
       ├── DOC-ICD-CORE-001 : Çekirdek Mimari (Medya Bağımsız, Adresleme, State Machine)
       └── DOC-ICD-DATA-002 : Standart Veri Tipleri & Konfigürasyon Parametreleri
                 │
[Seviye 2: Taşıma Katmanları (Transport)]
       ├── DOC-ICD-TRP-CANFD-005 : CAN-FD Taşıma Katmanı (29-bit CAN ID, BRS, DLC)
       ├── DOC-ICD-TRP-TCP-006   : Ethernet / TCP Taşıma Katmanı (Akış Çerçeveleme)
       └── DOC-ICD-TRP-LORA-007  : LoRa Telemetri Katmanı (Hava Çerçeveleme, ACK/Retry)
                 │
[Seviye 3: Sistem Çapında Servisler]
       ├── DOC-ICD-NET-003 : Ağ Yönetimi (SYSTEM Servisi, PING/PONG, Heartbeat)
       └── DOC-ICD-DIAG-004: Hata ve Arıza Yönetimi (DIAGNOSTIC Servisi, DTC Kodları)
                 │
[Seviye 4: Fonksiyonel Düğümler ve Servisler]
       ├── DOC-ICD-NODE-TRAC-008 : Çekiş (Traction) Sistemi Alt Sistemi
       ├── DOC-ICD-NODE-STR-009  : Direksiyon (Steering) Sistemi Alt Sistemi
       ├── DOC-ICD-NODE-BRK-010  : Fren (Brake) Sistemi Alt Sistemi
       ├── DOC-ICD-SRV-LGHT-011  : Aydınlatma (Lighting) Fonksiyonel Servisi
       └── DOC-ICD-NODE-BMS-012  : Batarya Yönetim Sistemi (BMS Main & Aux)
```

### Detaylı Doküman Kataloğu

| Doküman Kodu | Dosya Adı | Kapsam ve Açıklama |
|---|---|---|
| **`DOC-ICD-CORE-001`** | [ICD_Core_Architecture.pdf](ICD_Core_Architecture.pdf) | Medya bağımsız DEOS protokol çekirdeği, genel mesaj yapısı (`deos_message_t`), mantıksal adresleme, araç ana durum makinesi (`VEHICLE_STATE`, `VEHICLE_MODE`). |
| **`DOC-ICD-DATA-002`** | [ICD_Data_Types_Config.pdf](ICD_Data_Types_Config.pdf) | Standart atomik veri tipleri, `CONFIG_GET`/`CONFIG_SET` mekanizması, `param_id` kayıt defteri. |
| **`DOC-ICD-NET-003`** | [ICD_Network_Management.pdf](ICD_Network_Management.pdf) | SYSTEM servisi (`0x00`), PING/PONG mesajlaşması, canlılık takibi (Heartbeat) ve düğüm tanıma. |
| **`DOC-ICD-DIAG-004`** | [ICD_Fault_Management.pdf](ICD_Fault_Management.pdf) | DIAGNOSTIC servisi (`0x07`), DTC hata kodlama yapısı, arıza seviyeleri (`WARNING`, `CRITICAL`, `FATAL`). |
| **`DOC-ICD-TRP-CANFD-005`** | [ICD_CANFD_Transport.pdf](ICD_CANFD_Transport.pdf) | CAN-FD fiziksel katmanı, 29-bit Extended ID bit yerleşimi, baud rate kuralları, DLC padding kuralları. |
| **`DOC-ICD-TRP-ETH-006`** | [ICD_Ethernet_TCP.pdf](ICD_Ethernet_TCP.pdf) | Ethernet (TCP \& UDP) Taşıma Katmanı: Textual UI (TCP Port 5000) ve ROS 2 Bridge (UDP Port 5001) çerçeveleme, dinamik istemci keşfi ve paket anatomisi. |
| **`DOC-ICD-TRP-LORA-007`** | [ICD_LoRa_Telemetry.pdf](ICD_LoRa_Telemetry.pdf) | Yer kontrol telemetrisi için LoRa/UART senkronizasyon çerçevesi (`0xAA55`), hava süresi optimizasyonu, ACK/Retry protokolü. |
| **`DOC-ICD-NODE-TRAC-008`** | [ICD_Traction.pdf](ICD_Traction.pdf) | Çekiş kontrol düğümü (`0x10`) ve servisi (`0x01`): Tork talebi, hız geri bildirimi, motor sürücü telemetrisi. |
| **`DOC-ICD-NODE-STR-009`** | [ICD_Steering.pdf](ICD_Steering.pdf) | Direksiyon kontrol düğümü (`0x11`) ve servisi (`0x02`): Hedef açı, mevcut açı, tork sınırlama, kalibrasyon. |
| **`DOC-ICD-NODE-BRK-010`** | [ICD_Brake.pdf](ICD_Brake.pdf) | Elektronik frenleme düğümü (`0x12`) ve servisi (`0x03`): Fren basıncı, acil frenleme (EBS), aktüatör durumu. |
| **`DOC-ICD-SRV-LGHT-011`** | [ICD_Lighting.pdf](ICD_Lighting.pdf) | Aydınlatma servisi (`0x08`). Doğrudan Ana Araç Beyni (`DEOS_NODE_MAIN_STM32` / `0x03`) üzerinde koşan fonksiyonel servistir. |
| **`DOC-ICD-NODE-BMS-012`** | [ICD_BMS.pdf](ICD_BMS.pdf) | Batarya yönetim düğümleri (`0x20` Main, `0x21` Aux) ve servisi (`0x04`): Hücre voltajları, akım, sıcaklık, SOC/SOH, kontaktör kontrolü. |

---

## 3. Düğüm (Node) ve Servis (Service) Haritası

DEOS mimarisinde düğümler fiziksel/mantıksal adresleri temsil ederken; servisler yürütülen fonksiyonel yetenekleri ifade eder. Bir fiziksel mikrodenetleyici birden fazla mantıksal (hosted/virtual) düğüme ev sahipliği yapabilir (`deos_register_local_node`).

| Düğüm Adı | Node ID | Düğüm Türü | Barındırdığı Birincil Servisler | Açıklama |
|---|---|---|---|---|
| `DEOS_NODE_INVALID` | `0x00` | - | - | Geçersiz / Tanımsız Düğüm |
| `DEOS_NODE_TEXTUAL` | `0x01` | Sanal / Arayüz | SYSTEM (`0x00`), CONFIG (`0x06`) | Ethernet TCP (Port 5000) Konsol / CLI Arayüzü |
| `DEOS_NODE_GROUND_CONTROL` | `0x02` | Sanal / Uzak | SYSTEM (`0x00`), Telemetri | LoRa üzerinden Yer Kontrol İstasyonu |
| `DEOS_NODE_MAIN_STM32` | `0x03` | Fiziksel Cihaz | SYSTEM (`0x00`), LIGHTING (`0x08`), Router | Araç Ana Kontrolcüsü, Ağ Yönlendirici \& Aydınlatma |
| `DEOS_NODE_ROS_BRIDGE` | `0x04` | Sanal / Köprü | SYSTEM (`0x00`), Telemetri \& Otonom | Ethernet UDP (Port 5001) ROS 2 Köprüsü (`deos_ros_bridge`) |
| `DEOS_NODE_TRACTION` | `0x10` | Fiziksel ECU | TRACTION (`0x01`), SYSTEM (`0x00`) | Çekiş / İnvertör Düğümü |
| `DEOS_NODE_STEERING` | `0x11` | Fiziksel ECU | STEERING (`0x02`), SYSTEM (`0x00`) | Direksiyon Motor Kontrolcüsü |
| `DEOS_NODE_BRAKE` | `0x12` | Fiziksel ECU | BRAKE (`0x03`), SYSTEM (`0x00`) | Mekanik / Hidrolik Fren Kontrolcüsü |
| `DEOS_NODE_IMU` | `0x13` | Sanal / Hosted | SYSTEM (`0x00`), Sensörler (`0x07`) | STM32 Ana Kartı üzerinde koşan 6-Eksen IMU Düğümü |
| `DEOS_NODE_BMS_MAIN` | `0x20` | Sanal / Hosted | BMS (`0x04`), SYSTEM (`0x00`) | BMS Gateway üzerinde koşan Ana Batarya Daly Proxy Düğümü |
| `DEOS_NODE_BMS_AUX` | `0x21` | Sanal / Hosted | BMS (`0x04`), SYSTEM (`0x00`) | BMS Gateway üzerinde koşan Yardımcı Batarya Daly Proxy Düğümü |
| `DEOS_NODE_BMS_GATEWAY` | `0x30` | Fiziksel Cihaz | SYSTEM (`0x00`), Classic CAN Köprüsü | Classic CAN -- CAN-FD Ağ Geçidi (Gateway) |
| `DEOS_NODE_DIAG_TOOL` | `0xF0` | Sanal / Test | SYSTEM (`0x00`), DIAGNOSTIC (`0x07`) | Harici Teşhis / Test Cihazı (Ethernet) |
| `DEOS_NODE_CAN_BROADCAST` | `0xFE` | Mantıksal Yayın | Tüm Servisler | Yalnızca CAN-FD Hattı Yayını (Heartbeat vb.) |
| `DEOS_NODE_GLOBAL_BROADCAST` | `0xFF` | Mantıksal Yayın | Tüm Servisler | Tüm Fiziksel Hatlara Genel Yayın (E-STOP, State) |

> **Önemli Not (Aydınlatma Servisi):** Aydınlatma bağımsız bir mikrodenetleyici düğümü değildir. `DEOS_NODE_MAIN_STM32` (`0x03`) düğümüne gönderilen `DEOS_SERVICE_LIGHTING` (`0x08`) servis mesajları ile ana beyin üzerindeki transistör/röle sürücüleri tetiklenir.

---

## 4. Dokümanları Derleme (PDF Üretimi)

Dokümanlar LaTeX tabanlıdır (`pdflatex`). Dizin içerisindeki tüm dokümanları tek seferde derlemek ve oluşan geçici derleme dosyalarını (`.aux`, `.log`, `.out`, `.toc`) otomatik temizlemek için derleme betiği sağlanmıştır:

```bash
cd docs/icd
chmod +x build_icd.sh
./build_icd.sh
```

Betiğin başarılı çalışması sonucunda dizinde yalnızca kaynak `.tex` dosyaları ve üretilen nihai `.pdf` dosyaları muhafaza edilir.

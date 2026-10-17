# 🏎️ DEOS Firmware

DEOS Firmware, Zephyr RTOS üzerinde koşan, modüler donanım mimarisine ve gelişmiş iletişim (CAN-FD, LoRa, Ethernet vb.) altyapılarına sahip dağıtık araç içi gömülü kontrol yazılımıdır.

Proje, donanım bağımsız (hardware-agnostic) bir yapıda geliştirilmiş olup, Zephyr'in **T2 Workspace Topology (Application as a Manifest Repository)** standartlarına ve modüler Git Submodule mimarisine tam uyumludur.

---

## 🏛️ Dağıtık Sistem Mimarisi ve Ekosistem Ağacı

DEOS ekosistemi; gömülü mikrodenetleyiciler (STM32), otonomi bilgisayarı (Raspberry Pi / Jetson), CAN-FD omurgası (Backbone), LoRa telemetri ve yer kontrol istasyonundan oluşan dağıtık bir araç ağı mimarisine sahiptir:

```mermaid
graph TD
    classDef mcu fill:#1d4ed8,stroke:#60a5fa,stroke-width:3px,color:#ffffff;
    classDef host fill:#0f172a,stroke:#94a3b8,stroke-width:2px,color:#ffffff;
    classDef rf fill:#581c87,stroke:#c084fc,stroke-width:2px,color:#ffffff;
    classDef can fill:#065f46,stroke:#34d399,stroke-width:2px,color:#ffffff;
    classDef bms fill:#1e293b,stroke:#64748b,stroke-width:1.5px,color:#cbd5e1;
    classDef bus fill:#0284c7,stroke:#38bdf8,stroke-width:5px,color:#ffffff;

    %% 1. Otonom Hesaplama Ünitesi
    PI["🍓 Raspberry Pi / Jetson<br/><b>Otonom Sürüş & ROS 2</b>"]:::host
    PI <== "Ethernet" ==> MAIN

    %% 2. Merkez: Main STM32 Router
    MAIN["🧠 Main STM32<br/><b>Router & Gateway (deos-core)</b>"]:::mcu

    %% 3. Sol Kısım: Kumanda & Bluetooth
    JOY["🎮 Manuel Kumanda<br/><i>(Gamepad / RC Controller)</i>"]:::host
    BT["📶 Bluetooth Modülü<br/><i>(HC-05 / BLE)</i>"]:::rf
    JOY <.. "Bluetooth" ..> BT
    BT <== "UART" ==> MAIN

    %% 4. Sağ Kısım: LoRa & Ground Control (deos-ground-station)
    LORA["📡 LoRa Modülü<br/><i>(SX1262 Telemetri)</i>"]:::rf
    GCS["💻 Ground Control<br/><b><a href='https://github.com/DEUDEOS-Team/deos-ground-station'>deos-ground-station</a></b><br/><i>(Telemetri & GUI)</i>"]:::host
    MAIN <== "UART" ==> LORA
    LORA <.. "RF Link (LoRa)" ..> GCS

    %% 5. Alt Kısım: CAN-FD Backbone Veri Yolu
    MAIN <== "TX / RX" ==> BUS["══════════════════════════════════ CAN-FD Backbone ══════════════════════════════════"]:::bus

    %% 6. CAN Hattına Bağlı Düğümler
    BUS <==> TRACTION["⚙️ Traction<br/><b>Motor / İnvertör</b>"]:::can
    BUS <==> BRAKE["🛑 Brake<br/><b>Fren Sistemi</b>"]:::can
    BUS <==> STEERING["🎯 Steering<br/><b>Steer-by-Wire</b>"]:::can
    BUS <==> BMS_GW["🔋 BMS Gateway<br/><b>Batarya Ağ Geçidi</b>"]:::can

    %% 7. Batarya Hücre Paketleri
    BMS_GW <--> BMS1["🔋 BMS 1 (Master Paket)"]:::bms
    BMS_GW <--> BMS2["🔋 BMS 2 (Slave Paket)"]:::bms
```

---

## 📦 Ekosistem Depoları ve Git Alt Modülleri

DEOS projesi "Monorepo" karmaşasını önlemek amacıyla bağımsız depolara ayrılmıştır:

| Depo Adı | Türü | Sorumlu | Açıklama |
| :--- | :--- | :--- | :--- |
| [**`deos-core`**](https://github.com/DEUDEOS-Team/deos-core) | Alt Modül (`lib/deos_core`) | **Emir Arkalı** | STM32 üzerinde koşan yönlendirici, abonelik motoru ve arıza tablosu |
| [**`deos-protocol`**](https://github.com/DEUDEOS-Team/deos-protocol) | Alt Modül (`lib/deos_protocol`) | **Ortak Protokol** | ANSI C99 Universal Frame codec ve Python C-Bindings kütüphanesi |
| [**`deos-transport-can`**](https://github.com/DEUDEOS-Team/deos-transport-can) | Alt Modül (`lib/deos_transport_can`) | **Donanım / CAN Ekibi** | STM32 FDCAN sürücüsü ve 29-bit CAN ID filtreleme motoru |
| [**`deos-ground-station`**](https://github.com/DEUDEOS-Team/deos-ground-station) | Yer Kontrol İstasyonu | **Yer İstasyonu Ekibi** | PyQt tabanlı telemetri ve komuta kontrol arayüzü (LoRa RF) |
| [**`deos-docs`**](https://github.com/DEUDEOS-Team/deos-docs) | Alt Modül (`docs`) | **Dokümantasyon Ekibi** | Resmi LaTeX ICD şartnameleri ve ağ mimarisi kuralları |

---

## 🚀 Kurulum ve İlk Adımlar

DEOS Firmware, kendi içerisinde bir Zephyr manifestosu (`west.yml`) barındırır. Bu sayede gerekli olan Zephyr çekirdeği ve dış modüller otomatik olarak indirilir.

Projeyi bilgisayarınıza kurmak için standart `git clone` **yerine** aşağıdaki Zephyr komutlarını kullanmanız gerekir:

```bash
# 1. Projeyi indireceğiniz yeni bir çalışma alanı (workspace) oluşturun
west init -m https://github.com/DEUDEOS-Team/deos-firmware.git deos_workspace

# 2. Çalışma alanına girin
cd deos_workspace

# 3. Bağımlılıkları, Zephyr çekirdeğini ve modülleri otomatik indirin
west update
```

Bu komutlardan sonra Zephyr ortamınız ve gerekli tüm modüller inmiş ve derlenmeye hazır olacaktır.

---

## 🛠️ Proje Yönetimi (Tavsiye Edilen Araç)

Zephyr projelerinin ortam kurulumu, derlenmesi (build) ve karta yüklenmesi (flash) gibi işlemler bazen karmaşık olabilir. 

Tüm bu süreçleri otomatize etmek ve geliştirme hızınızı artırmak için **[Zephyr Project Manager](https://github.com/emirarkali/Zephyr-Project-Manager)** aracını bilgisayarınıza indirip kullanmanızı şiddetle tavsiye ederiz. Bu araç sayesinde terminalde uzun komutlar yazmadan projelerinizi kolayca yönetebilirsiniz.

---

## 🧩 Overlay (Donanım Soyutlama) Mimarisi

DEOS projesi, kodların farklı donanım kartlarında (örneğin NUCLEO-H563ZI veya NUCLEO-H7A3ZI-Q) **hiçbir C++ kodu değiştirilmeden** çalışabilmesi için kusursuz bir Device Tree (Overlay) mimarisine sahiptir.

Projeye yeni bir donanım veya kart eklerken aşağıdaki kurallara kesinlikle uyulmalıdır:

### 1. `app.overlay` (İskelet Dosyası)
Uygulamanın ana klasöründe bulunur. Görevi sadece projenin **donanımdan bağımsız iskeletini** kurmaktır.
- Burada fiziksel pin atamaları **YAPILMAZ** (Örn: `&gpioe 14` yazılamaz).
- Hangi UART kanalının (`usart1`, `lpuart1` vb.) kullanılacağı **BELİRTİLMEZ**.
- Sadece uygulamada kullanılacak sanal modüller (node) ve etiketler (label) tanımlanır.

### 2. `boards/` İçindeki Overlay Dosyaları (Karta Özel Dosyalar)
Elinizdeki fiziksel kartın adıyla aynı olan dosyalardır (Örn: `nucleo_h563zi.overlay`).
- `app.overlay` dosyasında oluşturulan iskeletin, **sizin kartınızdaki hangi fiziksel pinlere** takıldığını belirttiğiniz yerdir.
- UART portu seçimleri, baud rate hız ayarları, CAN-FD RX/TX pin atamaları tamamen bu dosyada yapılır.
- Derleme sırasında (`west build -b nucleo_h563zi`) Zephyr, önce `app.overlay` iskeletini okur, sonra gidip `boards/nucleo_h563zi.overlay` içindeki fiziksel pinleri bu iskelete giydirir.

Örnek bir derleme komutu:
```bash
west build -b nucleo_h563zi apps/main_node
```

> **Not:** Sistem mimarisinin derinlikleri, ağ iletişimi (network routing) ve sıfır gecikme (zero-latency) optimizasyonları hakkında daha fazla bilgi edinmek için lütfen `docs/DEOS_ARCHITECTURE_KNOWLEDGE.md` dosyasını okuyun.

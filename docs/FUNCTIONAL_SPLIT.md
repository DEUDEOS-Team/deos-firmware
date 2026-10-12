# DEOS Sistem Mimarisi: Fonksiyonel Ayrım ve Sorumluluk Matrisi (Functional Split)
**Sürüm:** `v0.1-DRAFT` (Erken Taslak)  
**Tarih:** 13 Ekim 2026  
**Yazar:** Emir Arkalı (`emirarkali07@gmail.com`)  
**Kapsam:** ROS 2 Otonom Sürüş, Ağ Köprüsü, Main STM32 (VCU), Aktüatör ECU'ları ve BMS Gateway

---

## 1. Amaç ve Kapsam

Bu doküman, DEOS otonom araç ekosisteminde yer alan farklı hesaplama ve kontrol katmanlarının **görev, yetki ve sorumluluk sınırlarını (Functional Split)** belirler. 

Temel amaç; **Otonom Sürüş (ROS 2 / IPC)**, **Merkezi Araç Kontrolcüsü (Main STM32)** ve **Alt Düğümler (Traction, Steering, Brake, BMS)** arasındaki görev karmaşasını önlemek, hesaplama yükünü doğru donanıma dağıtmak ve kritik güvenlik/failsafe sınırlarını netleştirmektir.

```
┌────────────────────────────────────────────────────────────────────────┐
│             KATMAN 1: OTONOM SÜRÜŞ BİLGİSAYARI (ROS 2 / IPC)           │
│     Algılama (LiDAR/Kamera), SLAM/Navigasyon, Yörünge Planlama         │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ SI Birimleri (m/s, rad)
                                    ▼ UDP 5001
┌────────────────────────────────────────────────────────────────────────┐
│             KATMAN 2: AĞ KÖPRÜSÜ (deos_ros_bridge)                     │
│      Birim Dönüşümü, Ackermann Kinematiği, 200ms Komut Bekçisi        │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ DEOS Frame (Length + Header + Payload)
                                    ▼ UDP 5001
┌────────────────────────────────────────────────────────────────────────┐
│             KATMAN 3: MERKEZİ ARAÇ KONTROLCÜSÜ (Main STM32 / VCU)      │
│  State Machine, Güvenlik Hakemi (E-STOP), Router, Aydınlatma / Sinyal  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ CAN-FD Omurgası (1M / 5M BRS)
                 ┌──────────────────┼──────────────────┐
                 ▼                  ▼                  ▼
┌─────────────────────────┐┌──────────────────┐┌─────────────────────────┐
│ KATMAN 4A: TRACTION ECU ││KATMAN 4B: STEERING││ KATMAN 4C: BRAKE ECU   │
│ Motor Akım/PID Döngüsü  ││Açı Geri Beslemesi ││ EBS Acil Frenleme      │
└─────────────────────────┘└──────────────────┘└─────────────────────────┘
```

---

## 2. Katmanlar Arası Sorumluluk Matrisi

| Fonksiyonel Alan | Otonom Bilgisayar (ROS 2) | Ağ Köprüsü (`deos_ros_bridge`) | Main STM32 (VCU) | Aktüatör / Sensör ECU'ları |
|---|---|---|---|---|
| **Çevre Algılama & SLAM** | **Tam Sorumlu** (LiDAR, Kamera, Harita) | İlgisiz | İlgisiz | İlgisiz |
| **Yörünge & Hız Planlama** | **Tam Sorumlu** (`/cmd_vel`) | İlgisiz | İlgisiz | İlgisiz |
| **Ackermann Kinematik Dönüşümü** | İlgisiz ($v, \omega$ üretir) | **Dönüştürücü** ($v, \omega \rightarrow v, \delta$) | Doğrulayıcı (Limit kontrolü) | İlgisiz |
| **Birim Ölçekleme (SI $\leftrightarrow$ Fixed-Point)** | İlgisiz (Saf SI birimleri) | **Tam Sorumlu** | İlgisiz (Fixed-point kullanır) | İlgisiz (Fixed-point kullanır) |
| **Motor Tork & Hız PID** | İlgisiz | İlgisiz | İlgisiz | **Traction ECU** (1 kHz iç çevrim) |
| **Direksiyon Açı PID & Konum**| İlgisiz | İlgisiz | İlgisiz | **Steering ECU** (Enkoder çevrimi) |
| **Fren Basınç & EBS Denetimi**| İlgisiz | İlgisiz | Karar Verici (E-STOP tetikleme) | **Brake ECU** (Valf/Motor sürme) |
| **Sinyal & Flaşör Mantığı** | **Niyet Bildirir** (`right = true`) | İletici | **Zamanlayıcı** (1 Hz Blinker donanımı) | İlgisiz |
| **Acil Durdurma (Hard E-STOP)** | İlgisiz | İlgisiz | **En Üst Otorite** (GPIO Kesmesi) | STO (Safe Torque Off) Uygulayıcı |
| **Watchdog & Canlılık Takibi** | Düğüm içi sağlık | 200 ms `/cmd_vel` bekçisi | **Merkezi Hakem** (ECU Heartbeat) | Kendi komut timeout kontrolü |
| **Durum Makinesi (State Machine)** | Durum İzleyici / Talepkâr | İletici | **Tek Karar Verici** | Durum Takipçisi |
| **Batarya (BMS) Yönetimi** | Telemetri İzleyici | İletici | İletici / Güvenlik Kapatması | **BMS Gateway** (Daly proxy) |

---

## 3. Kritik Sınırların ve Gri Alanların Çözümü

### 3.1. Aydınlatma ve Sinyal Lambası Mantığı (Blinker Split)
* **Kural:** ROS 2 otonom planlayıcısı periyodik lamba açıp kapatma (1 Hz yanıp sönme) **yapmaz**. Ağ trafiği bu tür rutinlerle meşgul edilmez.
* **Akış:**
  1. ROS 2 `/vehicle/signals/right` topic'ine `true` basar (`On Change`).
  2. `deos_ros_bridge`, UDP 5001 üzerinden Main STM32'ye `DEOS_CMD_LIGHTING_TURN_ON(DEOS_LIGHT_TURN_RIGHT)` komutunu fırlatır.
  3. Main STM32 üzerindeki `LedController`, dahili donanımsal timer ile sağ sinyali 1 Hz periyotla (%50 duty) yakıp söndürür.
  4. Sürüş tamamlandığında ROS 2 `false` basar ve STM32 donanımı kapatır.

### 3.2. Kinematik vs Motor Sürücü Sınırı
* **Kural:** ROS 2 aracı tek bir rijit cisim olarak modelleyerek doğrusal hız ($v = \text{linear.x}$) ve açısal sapma hızı ($\omega = \text{angular.z}$) üretir.
* **Akış:**
  1. `deos_ros_bridge`, aracın dingil mesafesini ($L$) kullanarak Ackermann açısını hesaplar:
     $$\delta = \arctan\left(\frac{L \cdot \omega}{v}\right)$$
  2. Elde edilen tekerlek açısı ($\delta$) dereceye ve $0.01^\circ$ ölçeğine çevrilerek `DEOS_NODE_STEERING` düğümüne yollanır.
  3. Hedef hız ($v$), $0.01\text{ m/s}$ ölçeğine çevrilerek `DEOS_NODE_TRACTION` düğümüne yollanır.
  4. Traction ECU kendi içindeki PID/FOC motor sürücüsüyle tekerlek torkunu üretir. Otonom taraf motorun iç akımından veya voltajından soyutlanmıştır.

### 3.3. Güvenlik Hakemliği ve Yetki Hiyerarşisi (Command Arbitration)

Aracın tekerleklerine giden nihai tork ve fren komutunda **asla tek bir kaynağa güvenilmez**. Çok katmanlı yetki hiyerarşisi uygulanır:

$$\text{Donanımsal E-STOP} > \text{Yer Kontrolü (LoRa)} > \text{Sistem Arızası (Watchdog)} > \text{Manuel Sürüş} > \text{Otonom Sürüş (ROS 2)}$$

1. **Donanımsal E-STOP (Öncelik: 1 - En Yüksek):**
   * Araçtaki mantar butona basıldığı anda Main STM32 mikrodenetleyicisi `SafetyManager` donanımsal kesmesiyle `DEOS_STATE_SAFE` durumuna geçer.
   * `0xFF` (Global Broadcast) ile Traction ve Brake ECU'larına acil duruş gider. ROS 2'nin bu durumu geçersiz kılma yetkisi **yoktur**.
2. **Yer İstasyonu Müdahalesi (Öncelik: 2):**
   * LoRa üzerinden gelen durdurma komutu ROS 2 hareket komutlarını anında ezer.
3. **Zaman Aşımı / Bağlantı Kopması Failsafe'i (Öncelik: 3):**
   * ROS 2 otonom düğümünden 200 ms boyunca yeni `/cmd_vel` gelmezse `deos_ros_bridge` hızı sıfırlar.
   * STM32 Main Node ile herhangi bir ECU arasında 500 ms Heartbeat kesilirse sistem `DEOS_STATE_FAULT` durumuna geçer.

---

## 4. Zamanlama ve Failsafe Matrisi

| Hata / Olay Durumu | Tespit Eden Katman | Algılama Süresi | Tetiklenen Failsafe Davranışı |
|---|---|:---:|---|
| **ROS 2 Düğümü Çökmesi** | `deos_ros_bridge` | $200\text{ ms}$ | Hedef hız $0\text{ m/s}$ yapılır, yumuşak duruş talep edilir. |
| **Ethernet Kablosu Kopması** | Main STM32 (UDP/TCP) | $500\text{ ms}$ | Otonom mod iptal edilir, araç `STANDBY` moduna çekilir. |
| **Traction ECU İletişim Kaybı**| Main STM32 (CAN-FD) | $500\text{ ms}$ | `DEOS_STATE_SAFE` ilan edilir, Brake ECU acil fren sıkar. |
| **Direksiyon Sensör Arızası** | Steering ECU | $<50\text{ ms}$ | Tork sıfırlanır, `DEOS_FAULT_ACTUATOR_ERROR` fault kodu fırlatılır. |
| **BMS Aşırı Sıcaklık / Düşük Voltaj**| BMS Gateway | $<100\text{ ms}$ | Kritik arıza bildirilir, kontaktörler kontrollü açılır. |
| **Fiziksel E-STOP Butonu** | Main STM32 GPIO | $<1\text{ ms}$ | Tüm aktüatörler enerjisiz bırakılır, EBS kilitlenir. |

---

## 5. Sonuç ve Geliştirme Yol Haritası

Bu fonksiyonel dağılım belgesi erken geliştirme aşaması (`v0.1-DRAFT`) için hazırlanmıştır. 

Sistem dinamik test aşamasına geçtikçe:
1. Ackermann kinematiğinde kayma oranları (Slip angle compensation),
2. Tork vektörleme (Torque Vectoring) sorumluluğunun Traction ECU'ya entegrasyonu,
3. Ultrasonik mesafe sensörlerinin otonom acil frenleme (AEB) eşikleri,
bu dokümana yeni alt bölümler olarak eklenecektir.

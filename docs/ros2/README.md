# DEOS ROS 2 Topic Arayüz Dokümantasyonu

Bu dizin, otonom sürüş yazılım katmanı (ROS 2) ile araç üstü gömülü kontrol altyapısı arasındaki topic tabanlı haberleşme sözleşmesini içerir.

Bu arayüzün gömülü sistem (firmware) tarafındaki karşılığı, **`eth_ros_bridge` (UDP Port 5001)** modülü ve [`DOC-ICD-TRP-ETH-006`](../icd/ICD_Ethernet_Transport.pdf) şartnamesidir.

---

## 1. Dokümanlar

* [**`ROS2_Topic_Interface.pdf`**](ROS2_Topic_Interface.pdf): Derlenmiş tam şartname dokümanı.
* [**`ROS2_Topic_Interface.tex`**](ROS2_Topic_Interface.tex): LaTeX kaynak dosyası.

---

## 2. Temel Topic Özeti

### ROS 2 $\rightarrow$ Bridge (Gömülü Sisteme Giden Komutlar)

| Topic | Mesaj Tipi | Frekans / Davranış | Açıklama |
|---|---|:---:|---|
| `/cmd_vel` | `geometry_msgs/msg/Twist` | 20 Hz (200ms timeout) | Doğrusal hız (`linear.x` m/s) ve açısal hız (`angular.z` rad/s) |
| `/vehicle/autonomy_enable` | `std_msgs/msg/Bool` | 5 Hz (500ms timeout) | Otonom sürüş aktif/pasif kontrolü |
| `/vehicle/motion_enable` | `std_msgs/msg/Bool` | 10 Hz (250ms timeout) | Fiziksel hareket izni kontrolü |
| `/vehicle/signals/right` | `std_msgs/msg/Bool` | On Change | Sağ sinyal lambası |
| `/vehicle/signals/left` | `std_msgs/msg/Bool` | On Change | Sol sinyal lambası |
| `/vehicle/signals/hazard` | `std_msgs/msg/Bool` | On Change | Dörtlü ikaz lambaları |
| `/vehicle/lights/brake` | `std_msgs/msg/Bool` | On Change | Fren lambaları |
| `/vehicle/lights/headlight` | `std_msgs/msg/Bool` | On Change | Ön farlar |

### Bridge $\rightarrow$ ROS 2 (Araçtan Otonoma Gelen Geri Bildirimler)

| Topic | Mesaj Tipi | Frekans | Açıklama |
|---|---|:---:|---|
| `/vehicle/state/speed` | `std_msgs/msg/Float32` | 20 Hz | Anlık araç hızı (m/s) |
| `/vehicle/state/steering_angle` | `std_msgs/msg/Float32` | 20 Hz | Anlık direksiyon açısı (rad) |
| `/sensors/imu/data` | `sensor_msgs/msg/Imu` | 50 Hz | 6-eksen ivme ve açısal hız verisi |
| `/sensors/distance/<sensor_name>` | `sensor_msgs/msg/Range` | 20 Hz | Mesafe / ultrasonik sensörler |
| `/vehicle/state_machine/state` | `deos_msgs/msg/VehicleState` | On Change | Araç genel durum makinesi (AUTONOMOUS, EMERGENCY vb.) |

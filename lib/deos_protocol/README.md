# DEOS Protocol (Data Link & Codec Layer)

`deos_protocol`, DEOS araç içi dağıtık haberleşme ekosisteminin **işletim sisteminden tamamen bağımsız, saf ANSI C99 (Zero-Dependency)** protokol katmanıdır.

Herhangi bir RTOS (Zephyr, FreeRTOS), dinamik bellek ayırıcı (`malloc`) veya POSIX kütüphanesine ihtiyaç duymaz. Bu sayede hem **STM32 mikrodenetleyicilerinde** hem de **Linux/ROS 2, Python ve Masaüstü Yer İstasyonu** yazılımlarında doğrudan derlenebilir.

---

## 1. Modülün Sorumlulukları ve İçeriği

| Dosya | Açıklama |
| :--- | :--- |
| `include/deos/deos_frame.h` | 9-baytlık Evrensel Çerçeve (Universal Frame) başlık yapısı, encode ve decode API'leri. |
| `include/deos/deos_types.h` | Çekirdek mesaj konteyneri (`deos_message_t`), konfigürasyon ve tüm ICD servis veri yükü (payload) yapıları. |
| `include/deos/deos_icd.h` | ICD Düğüm (Node), Servis (Service), Komut (Command), Öncelik (Priority) ve Hata (Fault) ID kayıtları. |
| `include/deos/deos_codec.h` | Taşınabilir Little-Endian serileştirme ve deserileştirme yardımcıları (`deos_put_u16_le`, `deos_get_u16_le` vb.). |
| `include/deos/deos_registry.h` | Reentrant, sıfır maliyetli ve deterministik mesaj işleyici kayıt tablosu (`deos_registry_t`). |
| `src/deos_frame.c` | 9-baytlık Evrensel Çerçeve (Universal Frame) başlık kodlama, uzunluk doğrulama ve string yardımcıları. |
| `src/deos_codec.c` | Donanım ve mimariden bağımsız bit kaydırmalı bayt paketleme implementasyonu. |
| `src/deos_registry.c` | Dinamik bellek kullanmayan, hızlı callback arama ve çalıştırma motoru. |

---

## 2. ROS 2 C++ Bridge Paketinde Kullanım

ROS 2 düğümünüzde DEOS mesajlarını çözmek ve ROS topic'lerine yayınlamak için `deos_core`'un tamamını bağlamanıza gerek yoktur. Sadece `deos_protocol`'ü bağlamanız yeterlidir:

### CMakeLists.txt (ROS 2 Paketi)
```cmake
cmake_minimum_required(VERSION 3.15)
project(deos_ros_bridge)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

# DEOS saf C protokol kütüphanesini projeye dahil et
add_subdirectory(/path/to/deos_firmware/lib/deos_protocol deos_protocol_build)

add_executable(ros_bridge_node src/ros_bridge_node.cpp)
target_link_libraries(ros_bridge_node PRIVATE deos_protocol)
ament_target_dependencies(ros_bridge_node rclcpp sensor_msgs)

install(TARGETS ros_bridge_node DESTINATION lib/${PROJECT_NAME})
ament_package()
```

### C++ Düğüm Kodu Örneği
```cpp
#include <rclcpp/rclcpp.hpp>
#include <deos/deos_frame.h>
#include <deos/deos_types.h>
#include <deos/deos_icd.h>
#include <deos/deos_registry.h>

class RosBridgeNode : public rclcpp::Node {
public:
    RosBridgeNode() : Node("deos_ros_bridge") {
        // Handler Registry'yi başlat
        deos_registry_init(&m_registry);

        // İlgilendiğimiz telemetrileri ve komutları callback olarak kaydet
        deos_registry_register(&m_registry,
                               DEOS_CLASS_STATUS,
                               DEOS_SERVICE_TRACTION,
                               DEOS_CMD_TRACTION_GET_STATUS,
                               &RosBridgeNode::on_traction_status, this);
    }

    // UDP / Serial hattından ham baytlar geldiğinde:
    void on_raw_bytes_received(const uint8_t *buf, size_t len) {
        deos_message_t msg;
        if (deos_frame_decode(buf, len, &msg) == 0) {
            // Kayıtlı callback'leri otomatik tetikler
            deos_registry_dispatch(&m_registry, &msg);
        }
    }

private:
    static int on_traction_status(const deos_message_t *msg, void *user_data) {
        auto *self = static_cast<RosBridgeNode*>(user_data);
        if (msg->payload_len >= sizeof(struct deos_traction_target_payload)) {
            auto *payload = reinterpret_cast<const struct deos_traction_target_payload*>(msg->payload);
            // ROS topic yayını yap
            RCLCPP_INFO(self->get_logger(), "Hedef Hız: %d", payload->target_speed);
        }
        return 0;
    }

    deos_registry_t m_registry;
};
```

---

## 3. Python C-Bindings ile Kullanım (Yer İstasyonu & Scripting)

`deos_protocol`, derlenen `libdeos_protocol.so` paylaşımlı kütüphanesini doğrudan `ctypes` üzerinden çağıran resmi bir Python sarmalayıcısına (`lib/deos_protocol/python/deos.py`) sahiptir. Bu sayede Python kodu protokolü sıfırdan yazmaz; doğrudan C kodundaki encode, decode, codec ve registry fonksiyonlarını çalıştırır.

### Örnek 1: Yer Kontrol İstasyonu (GCS) UDP Dinleyici ve Komut Gönderici
Gerçek bir yer istasyonu veya telemetri izleme uygulamasında UDP soketi üzerinden araçla haberleşme:

```python
import socket
import threading
import time
import sys

# DEOS Python modülünü dahil et
sys.path.append("/path/to/deos_firmware/lib/deos_protocol/python")
import deos

# 1. C Handler Registry'yi başlat
registry = deos.DeosRegistry()

# 2. Telemetri ve durum dinleyicilerini (callback) tanımla
def on_traction_telemetry(msg: deos.DeosMessage):
    # C codec fonksiyonları ile int16 Little-Endian veriyi çöz:
    target_speed = deos.get_i16_le(msg.payload[0:2])
    actual_speed = deos.get_i16_le(msg.payload[2:4])
    print(f"[{msg.source_name}] Hedef: {target_speed} RPM | Anlık: {actual_speed} RPM")

def on_pong_response(msg: deos.DeosMessage):
    ping_id = deos.get_u32_le(msg.payload)
    print(f"[{msg.source_name}] PONG alındı! Ping ID: {ping_id}")

# Callback fonksiyonlarını C dispatcher motoruna kaydet:
registry.register(deos.CLASS_STATUS, deos.SERVICE_TRACTION,
                  deos.CMD_TRACTION_GET_STATUS, on_traction_telemetry)
registry.register(deos.CLASS_NETWORK, deos.SERVICE_SYSTEM,
                  deos.CMD_SYSTEM_PONG, on_pong_response)

# 3. UDP Soketi Aç
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5000))
VEHICLE_ADDR = ("192.168.1.100", 5000)

# 4. Arka Planda Gelen Paketleri Dinleyen Rx Thread'i
def rx_loop():
    while True:
        data, addr = sock.recvfrom(1024)
        # Doğrudan C deos_frame_decode() fonksiyonunu çalıştırır:
        msg = deos.DeosMessage.decode(data)
        if msg:
            # Doğrudan C deos_registry_dispatch() fonksiyonunu çalıştırır:
            registry.dispatch(msg)

threading.Thread(target=rx_loop, daemon=True).start()

# 5. Araçtaki Çekiş (Traction) Düğümüne Hız Komutu Gönder (Tx)
def send_speed_command(target_rpm: int):
    # C codec fonksiyonu ile int16Little-Endian paketle:
    speed_payload = deos.put_i16_le(target_rpm)
    
    cmd = deos.DeosMessage(
        destination=deos.NODE_TRACTION,
        source=deos.NODE_GROUND_CONTROL,
        priority=deos.PRIO_CONTROL,
        message_class=deos.CLASS_COMMAND,
        service=deos.SERVICE_TRACTION,
        command=deos.CMD_TRACTION_SET_TARGET_SPEED,
        sequence=1,
        payload=speed_payload
    )
    # Doğrudan C deos_frame_encode() fonksiyonunu çalıştırır:
    wire_bytes = cmd.encode()
    sock.sendto(wire_bytes, VEHICLE_ADDR)
    print(f"[{cmd.destination_name}] Hız Komutu Gönderildi: {target_rpm} RPM")

# Test komutu gönder
send_speed_command(1500)
```

---

### Örnek 2: ROS 2 Python (rclpy) Köprü Düğümü
DEOS paketlerini ROS 2 `sensor_msgs` veya standart topic'lere dönüştürmek için:

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import Int16
import deos

class DeosRos2BridgeNode(Node):
    def __init__(self):
        super().__init__('deos_ros2_bridge')
        self.publisher_ = self.create_publisher(Int16, 'vehicle/actual_speed', 10)
        
        # C Registry mekanizmasını başlat
        self.registry = deos.DeosRegistry()
        self.registry.register(
            deos.CLASS_STATUS,
            deos.SERVICE_TRACTION,
            deos.CMD_TRACTION_GET_STATUS,
            self.on_traction_status
        )

    def on_traction_status(self, msg: deos.DeosMessage):
        # C deos_get_i16_le() ile veriyi al ve ROS topic'ine yayınla
        speed = deos.get_i16_le(msg.payload[2:4])
        ros_msg = Int16(data=speed)
        self.publisher_.publish(ros_msg)

    def on_network_packet_received(self, raw_bytes: bytes):
        # Gelen ham baytları C kütüphanesiyle çöz ve otomatik dispatch et
        msg = deos.DeosMessage.decode(raw_bytes)  # C deos_frame_decode()
        if msg:
            self.registry.dispatch(msg)           # C deos_registry_dispatch()
```

---

## 4. Doğrulama ve Masaüstü Testleri

Masaüstünde Zephyr olmadan saf C/C++ ve Python olarak test etmek için:

### C++ Masaüstü Testi
```bash
mkdir -p tests/desktop/build
cd tests/desktop/build
cmake ..
make
./desktop_protocol_test
```

### Python C-Binding Testi
```bash
python3 tests/desktop/test_deos_protocol.py
```

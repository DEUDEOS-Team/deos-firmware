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

## 3. Python ile Kullanım (Yer Kontrol İstasyonu / Scripting)

`deos_protocol` çerçeve formatı, Python tarafında ek bir harici kütüphane gerektirmeden standart `struct` modülü ile doğrudan kullanılabilir.

### Saf Python Örneği (0 Bağımlılık)
```python
import struct
from dataclasses import dataclass
from typing import Optional

DEOS_UNIVERSAL_HEADER_SIZE = 9
DEOS_PROTOCOL_VERSION = 0x11

@dataclass
class DeosMessage:
    destination: int = 0
    source: int = 0
    priority: int = 4
    version: int = DEOS_PROTOCOL_VERSION
    message_class: int = 0
    service: int = 0
    command: int = 0
    sequence: int = 0
    payload: bytes = b""

    def encode(self) -> bytes:
        """9-baytlık Evrensel Çerçeve başlığı ve payload'ı paketler."""
        header = struct.pack(
            "<BBBBBBBBB",
            self.destination, self.source, self.priority, self.version,
            self.message_class, self.service, self.command, self.sequence,
            len(self.payload)
        )
        return header + self.payload

    @classmethod
    def decode(cls, data: bytes) -> Optional["DeosMessage"]:
        """Ham bayt dizisinden mesajı çözer."""
        if len(data) < DEOS_UNIVERSAL_HEADER_SIZE:
            return None
        dest, src, prio, ver, m_class, svc, cmd, seq, plen = struct.unpack(
            "<BBBBBBBBB", data[:DEOS_UNIVERSAL_HEADER_SIZE]
        )
        if len(data) < DEOS_UNIVERSAL_HEADER_SIZE + plen:
            return None
        return cls(dest, src, prio, ver, m_class, svc, cmd, seq, data[9:9+plen])

# Örnek: Yer İstasyonundan Araç Çekiş Düğümüne Hedef Hız Komutu Gönderme
target_speed = 1500  # RPM (int16_t)
payload = struct.pack("<h", target_speed)

msg = DeosMessage(
    destination=0x10,           # DEOS_NODE_TRACTION
    source=0x02,                # DEOS_NODE_GROUND_CONTROL
    priority=2,                 # DEOS_PRIO_CONTROL
    message_class=0x01,         # DEOS_CLASS_COMMAND
    service=0x01,               # DEOS_SERVICE_TRACTION
    command=0x01,               # DEOS_CMD_TRACTION_SET_TARGET_SPEED
    sequence=1,
    payload=payload
)

wire_bytes = msg.encode()
# wire_bytes doğrudan UDP soketine, LoRa framing'e veya CAN arayüzüne gönderilebilir!
```

> [!TIP]
> İstenirse `libdeos_protocol` paylaşımlı kütüphane (`.so`) olarak derlenip Python `ctypes` üzerinden de doğrudan C fonksiyonları çağrılabilir.

---

## 4. Doğrulama ve Masaüstü Testleri

### C++ Masaüstü Testi
```bash
mkdir -p tests/desktop/build
cd tests/desktop/build
cmake ..
make
./desktop_protocol_test
```

### Python Masaüstü Testi
```bash
python3 tests/desktop/test_deos_protocol.py
```

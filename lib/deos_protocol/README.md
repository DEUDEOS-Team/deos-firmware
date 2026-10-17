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
| `src/deos_frame.c` | CRC16-CCITT ve çerçeve doğrulama/kodlama mantığı. |
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

    deos_dispatcher_t m_dispatcher;
};
```

---

## 3. Doğrulama ve Masaüstü Testleri

Masaüstünde Zephyr olmadan saf C/C++ olarak test etmek için:

```bash
mkdir -p tests/desktop/build
cd tests/desktop/build
cmake ..
make
./desktop_protocol_test
```

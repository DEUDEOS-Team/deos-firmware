#!/usr/bin/env python3
"""
DEOS Protocol Python Referans ve Doğrulama Betiği
Bu modül, DEOS Evrensel Çerçeve (Universal Frame 9+N bayt) yapısını
saf Python (standart library struct) ile kodlama, çözme ve callback tablosu (Registry) mantığını gösterir.
"""

import struct
from dataclasses import dataclass
from typing import Callable, Dict, Tuple, Optional

# ICD Sabitleri (include/deos/deos_icd.h ile birebir eşdeğer)
DEOS_UNIVERSAL_HEADER_SIZE = 9
DEOS_PROTOCOL_VERSION = 0x11
DEOS_MAX_PAYLOAD_LEN = 55

# Node ID Tanımları
NODE_GROUND_CONTROL = 0x02
NODE_MAIN_STM32     = 0x03
NODE_ROS_BRIDGE     = 0x04
NODE_TRACTION       = 0x10
NODE_STEERING       = 0x11
NODE_BRAKE          = 0x12
NODE_GLOBAL_BCAST   = 0xFF

# Mesaj Sınıfları (Message Class)
CLASS_SAFETY   = 0x00
CLASS_COMMAND  = 0x01
CLASS_STATUS   = 0x02
CLASS_EVENT    = 0x03
CLASS_REQUEST  = 0x04
CLASS_RESPONSE = 0x05
CLASS_CONFIG   = 0x06
CLASS_NETWORK  = 0x07

# Servis ID Tanımları
SERVICE_SYSTEM   = 0x00
SERVICE_TRACTION = 0x01
SERVICE_STEERING = 0x02
SERVICE_BRAKE    = 0x03
SERVICE_BMS      = 0x04

# Komut Tanımları
CMD_SYSTEM_PING               = 0x08
CMD_SYSTEM_PONG               = 0x09
CMD_TRACTION_SET_TARGET_SPEED = 0x01
CMD_TRACTION_GET_STATUS       = 0x02


@dataclass
class DeosMessage:
    """DEOS Çekirdek Mesaj Yapısı (deos_message_t)"""
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
        """
        Mesajı 9-baytlık Universal Frame başlığı ve payload olarak baytlara serileştirir.
        Döndürülen veri doğrudan UDP soketine, CAN-FD'ye veya LoRa paketleyicisine verilebilir.
        """
        payload_len = len(self.payload)
        if payload_len > DEOS_MAX_PAYLOAD_LEN:
            raise ValueError(f"Payload boyutu izin verilen maksimum {DEOS_MAX_PAYLOAD_LEN} bayttan büyük: {payload_len}")

        header = struct.pack(
            "<BBBBBBBBB",
            self.destination & 0xFF,
            self.source & 0xFF,
            self.priority & 0xFF,
            self.version & 0xFF,
            self.message_class & 0xFF,
            self.service & 0xFF,
            self.command & 0xFF,
            self.sequence & 0xFF,
            payload_len & 0xFF,
        )
        return header + self.payload

    @classmethod
    def decode(cls, data: bytes) -> Optional["DeosMessage"]:
        """
        Gelen ham bayt akışından Universal Frame yapısını çözer.
        """
        if len(data) < DEOS_UNIVERSAL_HEADER_SIZE:
            return None

        dest, src, prio, ver, m_class, svc, cmd, seq, plen = struct.unpack(
            "<BBBBBBBBB", data[:DEOS_UNIVERSAL_HEADER_SIZE]
        )

        total_expected = DEOS_UNIVERSAL_HEADER_SIZE + plen
        if len(data) < total_expected:
            return None  # Çerçeve henüz tamamlanmamış

        payload = data[DEOS_UNIVERSAL_HEADER_SIZE:total_expected]
        return cls(
            destination=dest,
            source=src,
            priority=prio,
            version=ver,
            message_class=m_class,
            service=svc,
            command=cmd,
            sequence=seq,
            payload=payload,
        )


class DeosRegistry:
    """
    C kütüphanesindeki `deos_registry_t` mekanizmasının saf Python karşılığı.
    (Class, Service, Command) üçlüsüne göre eşleşen callback fonksiyonunu çağırır.
    """
    def __init__(self):
        self._handlers: Dict[Tuple[int, int, int], Callable[[DeosMessage], None]] = {}

    def register(self, message_class: int, service: int, command: int,
                 callback: Callable[[DeosMessage], None]):
        self._handlers[(message_class, service, command)] = callback

    def dispatch(self, msg: DeosMessage) -> bool:
        handler = self._handlers.get((msg.message_class, msg.service, msg.command))
        if handler:
            handler(msg)
            return True
        return False


# --- Test & Örnek Kullanım Senaryosu ---
def test_protocol_python():
    print("=" * 60)
    print("DEOS Protocol Saf Python Doğrulama Testi")
    print("=" * 60)

    received_data = {}

    def on_traction_status(msg: DeosMessage):
        # Payload içinde int16 hedef hız ve int16 anlık hız olduğunu varsayalım (Little-Endian)
        if len(msg.payload) >= 4:
            target_speed, current_speed = struct.unpack("<hh", msg.payload[:4])
            received_data["target_speed"] = target_speed
            received_data["current_speed"] = current_speed
            print(f"[CALLBACK] Traction Durumu Alındı -> Hedef Hız: {target_speed} RPM, Mevcut Hız: {current_speed} RPM")

    # 1. Registry oluştur ve abone ol
    registry = DeosRegistry()
    registry.register(CLASS_STATUS, SERVICE_TRACTION, CMD_TRACTION_GET_STATUS, on_traction_status)

    # 2. Örnek bir Traction Durum Mesajı paketle (Örn: STM32'den ROS Bridge'e gelen telemetri)
    payload_data = struct.pack("<hh", 1500, 1492)  # Target: 1500 RPM, Actual: 1492 RPM
    tx_msg = DeosMessage(
        destination=NODE_ROS_BRIDGE,
        source=NODE_TRACTION,
        priority=3,
        version=DEOS_PROTOCOL_VERSION,
        message_class=CLASS_STATUS,
        service=SERVICE_TRACTION,
        command=CMD_TRACTION_GET_STATUS,
        sequence=101,
        payload=payload_data,
    )

    encoded_bytes = tx_msg.encode()
    print(f"[TX] Kodlanan Çerçeve Boyutu: {len(encoded_bytes)} bayt")
    print(f"[TX] Ham Veri (Hex): {encoded_bytes.hex()}")

    assert len(encoded_bytes) == DEOS_UNIVERSAL_HEADER_SIZE + len(payload_data)
    assert encoded_bytes[0] == NODE_ROS_BRIDGE
    assert encoded_bytes[1] == NODE_TRACTION

    # 3. Alıcı tarafında çöz (Decode) ve Registry üzerinden tetikle
    rx_msg = DeosMessage.decode(encoded_bytes)
    assert rx_msg is not None
    assert rx_msg.destination == NODE_ROS_BRIDGE
    assert rx_msg.source == NODE_TRACTION
    assert rx_msg.sequence == 101

    dispatched = registry.dispatch(rx_msg)
    assert dispatched is True
    assert received_data.get("target_speed") == 1500
    assert received_data.get("current_speed") == 1492

    print("[BAŞARILI] Python encode, decode ve registry dispatch testi eksiksiz geçti!\n")


if __name__ == "__main__":
    test_protocol_python()

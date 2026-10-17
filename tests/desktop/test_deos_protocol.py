#!/usr/bin/env python3
"""
DEOS Protocol Python Test ve Doğrulama Betiği
Bu test, doğrudan C kütüphanesini (libdeos_protocol.so) çağıran Python bağlayıcısını (deos.py) test eder.
"""

import sys
from pathlib import Path

# lib/deos_protocol/python dizinini sys.path'e ekle
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "lib/deos_protocol/python"))

import deos

def test_c_library_bindings():
    print("=" * 65)
    print("DEOS Protocol - C Kütüphanesini (libdeos_protocol.so) Çağıran Test")
    print("=" * 65)

    # 1. C String İsim Çözücülerini Doğrula
    assert deos.DeosMessage(source=deos.NODE_MAIN_STM32).source_name == "MAIN_STM32"
    assert deos.DeosMessage(source=deos.NODE_TRACTION).source_name == "TRACTION"
    assert deos.DeosMessage(source=deos.NODE_GROUND_CONTROL).source_name == "GROUND_CONTROL"
    assert deos.DeosMessage(service=deos.SERVICE_TRACTION).service_name == "TRACTION"
    assert deos.DeosMessage(service=deos.SERVICE_TRACTION, command=deos.CMD_TRACTION_SET_TARGET_SPEED).command_name == "SET_TARGET_SPEED"
    print("[1] C String Lookup Fonksiyonları (deos_node_name, deos_command_name vb.) BAŞARILI.")

    # 2. C Little-Endian Codec Fonksiyonlarını Doğrula
    test_speed = 1500  # RPM
    encoded_speed_bytes = deos.put_i16_le(test_speed)
    decoded_speed = deos.get_i16_le(encoded_speed_bytes)
    assert decoded_speed == 1500
    assert encoded_speed_bytes == b'\xdc\x05'
    print(f"[2] C Codec Fonksiyonları (deos_put_i16_le, deos_get_i16_le) BAŞARILI: {decoded_speed} RPM -> {encoded_speed_bytes.hex()}")

    # 3. C Fonksiyonu `deos_frame_encode` ile Mesaj Paketleme
    tx_msg = deos.DeosMessage(
        destination=deos.NODE_TRACTION,
        source=deos.NODE_GROUND_CONTROL,
        priority=deos.PRIO_CONTROL,
        version=deos.DEOS_PROTOCOL_VERSION,
        message_class=deos.CLASS_COMMAND,
        service=deos.SERVICE_TRACTION,
        command=deos.CMD_TRACTION_SET_TARGET_SPEED,
        sequence=42,
        payload=encoded_speed_bytes,
    )

    wire_bytes = tx_msg.encode()
    print(f"[3] C deos_frame_encode Çağrıldı! Oluşan Tel Boyutu: {len(wire_bytes)} bayt")
    print(f"    Ham Veri (Hex): {wire_bytes.hex()}")

    # Universal Frame 9-bayt Başlık Doğrulaması:
    # Dest: 0x10, Src: 0x02, Prio: 0x02, Ver: 0x11, Class: 0x01, Svc: 0x01, Cmd: 0x01, Seq: 42 (0x2A), Len: 2 (0x02), Payload: dc 05
    assert len(wire_bytes) == 11
    assert wire_bytes == bytes.fromhex("100202110101012a02dc05")

    # 4. C Fonksiyonu `deos_frame_decode` ile Mesaj Ayrıştırma
    rx_msg = deos.DeosMessage.decode(wire_bytes)
    assert rx_msg is not None
    assert rx_msg.destination == deos.NODE_TRACTION
    assert rx_msg.source == deos.NODE_GROUND_CONTROL
    assert rx_msg.sequence == 42
    assert rx_msg.payload == b'\xdc\x05'
    print(f"[4] C deos_frame_decode Çağrıldı! Çözülen Mesaj: {rx_msg}")

    # 5. C Motoru `deos_registry_register` ve `deos_registry_dispatch` Doğrulaması
    registry = deos.DeosRegistry()
    received_in_callback = []

    def on_traction_speed_command(msg: deos.DeosMessage):
        speed = deos.get_i16_le(msg.payload)
        received_in_callback.append(speed)
        print(f"    >>> Python Callback Tetiklendi (C Dispatcher Üzerinden)! Alınan Hız: {speed} RPM")

    # Python fonksiyonunu C motoruna bağla:
    reg_ret = registry.register(
        message_class=deos.CLASS_COMMAND,
        service=deos.SERVICE_TRACTION,
        command=deos.CMD_TRACTION_SET_TARGET_SPEED,
        callback=on_traction_speed_command,
        target_node=deos.NODE_TRACTION
    )
    assert reg_ret == 0

    # C dispatcher'ını tetikle:
    dispatch_ret = registry.dispatch(rx_msg)
    assert dispatch_ret == 1  # 1 adet callback eşleşti ve çalıştı
    assert len(received_in_callback) == 1
    assert received_in_callback[0] == 1500
    print("[5] C deos_registry_register ve deos_registry_dispatch Doğrulaması EKSİKSİZ GEÇTİ!")

    print("\n" + "=" * 65)
    print("SONUÇ: Python, libdeos_protocol C kütüphanesini %100 başarıyla kullandı!")
    print("=" * 65 + "\n")

if __name__ == "__main__":
    test_c_library_bindings()

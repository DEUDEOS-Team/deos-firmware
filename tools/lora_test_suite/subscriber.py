#!/usr/bin/env python3
"""
DEOS ICD v1.1 - Telemetri ve Durum Abonelik Aracı (Subscription Client)
Yer Kontrol (DEOS_NODE_GROUND_CONTROL = 0x02) veya herhangi bir araçtan LoRa / UART
üzerinden diğer düğümlere periyodik veya Değişimde (On-Change) telemetri aboneliği açar.
"""

import serial
import struct
import time
import threading
import sys
import argparse
from datetime import datetime

def tprint(*args, **kwargs):
    msg = " ".join(map(str, args))
    print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {msg}", **kwargs)

# ============================================================================
# DEOS Protocol Definitions (ICD v1.1)
# ============================================================================

# Düğüm Kimlikleri (Node IDs)
DEOS_NODE_GROUND_CONTROL   = 0x02
DEOS_NODE_MAIN_STM32       = 0x03
DEOS_NODE_TRACTION         = 0x10
DEOS_NODE_STEERING         = 0x11
DEOS_NODE_BRAKE            = 0x12
DEOS_NODE_BMS_MAIN         = 0x20
DEOS_NODE_BMS_AUX          = 0x21
DEOS_NODE_BMS_GATEWAY      = 0x30
DEOS_NODE_DIAG_TOOL        = 0xF0
DEOS_NODE_CAN_BROADCAST    = 0xFE
DEOS_NODE_GLOBAL_BROADCAST = 0xFF

NODE_NAMES = {
    DEOS_NODE_GROUND_CONTROL:   "GROUND_CONTROL (Yer Kontrol)",
    DEOS_NODE_MAIN_STM32:       "MAIN_STM32 (Ana Beyin)",
    DEOS_NODE_TRACTION:         "TRACTION (Motor/Çekiş)",
    DEOS_NODE_STEERING:         "STEERING (Direksiyon)",
    DEOS_NODE_BRAKE:            "BRAKE (Fren)",
    DEOS_NODE_BMS_MAIN:         "BMS_MAIN (Ana Batarya)",
    DEOS_NODE_BMS_AUX:          "BMS_AUX (Yedek Batarya)",
    DEOS_NODE_BMS_GATEWAY:      "BMS_GATEWAY",
    DEOS_NODE_DIAG_TOOL:        "DIAG_TOOL",
    DEOS_NODE_CAN_BROADCAST:    "CAN_BROADCAST",
    DEOS_NODE_GLOBAL_BROADCAST: "GLOBAL_BROADCAST"
}

# Öncelikler (Priorities)
DEOS_PRIO_EMERGENCY = 0
DEOS_PRIO_CONTROL   = 2
DEOS_PRIO_STATUS    = 3
DEOS_PRIO_CONFIG    = 4
DEOS_PRIO_NETWORK   = 5

# Mesaj Sınıfları (Message Classes)
DEOS_CLASS_COMMAND  = 0x01
DEOS_CLASS_STATUS   = 0x03
DEOS_CLASS_REQUEST  = 0x04
DEOS_CLASS_RESPONSE = 0x05
DEOS_CLASS_EVENT    = 0x06
DEOS_CLASS_NETWORK  = 0x07

# Servisler (Services)
DEOS_SERVICE_SYSTEM     = 0x00
DEOS_SERVICE_TRACTION   = 0x01
DEOS_SERVICE_STEERING   = 0x02
DEOS_SERVICE_BRAKE      = 0x03
DEOS_SERVICE_BMS        = 0x04
DEOS_SERVICE_SAFETY     = 0x05
DEOS_SERVICE_DIAGNOSTIC = 0x06
DEOS_SERVICE_SENSORS    = 0x07
DEOS_SERVICE_LIGHTING   = 0x08

SERVICE_NAMES = {
    DEOS_SERVICE_SYSTEM:     "SYSTEM",
    DEOS_SERVICE_TRACTION:   "TRACTION",
    DEOS_SERVICE_STEERING:   "STEERING",
    DEOS_SERVICE_BRAKE:      "BRAKE",
    DEOS_SERVICE_BMS:        "BMS",
    DEOS_SERVICE_SAFETY:     "SAFETY",
    DEOS_SERVICE_DIAGNOSTIC: "DIAGNOSTIC",
    DEOS_SERVICE_SENSORS:    "SENSORS",
    DEOS_SERVICE_LIGHTING:   "LIGHTING"
}

# Komutlar (Commands)
DEOS_CMD_SYSTEM_GET_STATE          = 0x01
DEOS_CMD_SYSTEM_SET_STATE          = 0x02
DEOS_CMD_SYSTEM_GET_NODE_INFO      = 0x06
DEOS_CMD_SYSTEM_HEARTBEAT          = 0x07
DEOS_CMD_SYSTEM_PING               = 0x08
DEOS_CMD_SYSTEM_PONG               = 0x09
DEOS_CMD_SYSTEM_SUBSCRIBE          = 0x0A
DEOS_CMD_SYSTEM_UNSUBSCRIBE        = 0x0B
DEOS_CMD_SYSTEM_GET_SUBSCRIPTIONS  = 0x0C

DEOS_CMD_BMS_GET_STATUS            = 0x01
DEOS_CMD_BMS_STATUS                = 0x02
DEOS_CMD_BMS_GET_CELL_VOLTAGES     = 0x03
DEOS_CMD_BMS_CELL_VOLTAGES         = 0x04
DEOS_CMD_BMS_GET_BALANCING         = 0x07
DEOS_CMD_BMS_BALANCING             = 0x08

DEOS_CMD_TRACTION_SET_TARGET_SPEED = 0x01
DEOS_CMD_TRACTION_GET_STATUS       = 0x02

DEOS_CMD_LIGHTING_TURN_ON          = 0x01
DEOS_CMD_LIGHTING_TURN_OFF         = 0x02
DEOS_CMD_LIGHTING_STATUS           = 0x03
DEOS_CMD_LIGHTING_GET_STATUS       = 0x04

DEOS_CMD_DIAG_GET_FAULTS           = 0x02

# Özel Periyot Değeri: Sadece Durum Değiştiğinde Gönder (On-Change)
DEOS_INTERVAL_ON_CHANGE = 0xFFFF

# Sonuç Kodları (Result Codes)
RESULT_CODES = {
    0x00: "SUCCESS",
    0x01: "UNSUPPORTED",
    0x02: "INVALID_PARAMETER",
    0x03: "OUT_OF_RANGE",
    0x04: "NOT_ALLOWED",
    0x05: "BUSY",
    0x06: "NOT_READY",
    0x07: "INTERNAL_ERROR"
}

# Araç Durumları (States)
STATE_NAMES = {
    0x00: "INIT",
    0x01: "STANDBY",
    0x02: "READY",
    0x03: "ACTIVE",
    0x04: "CALIBRATING",
    0x05: "SAFE",
    0x06: "FAULT",
    0xFF: "UNKNOWN"
}

# LoRa UART Protokol Sabitleri
LORA_MAGIC_1 = 0xAA
LORA_MAGIC_2 = 0x55
LORA_FLAG_NONE = 0x00
LORA_FLAG_ACK_REQ = 0x01
LORA_FLAG_IS_ACK = 0x02

def crc16_ccitt(crc, data):
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc

# ============================================================================
# DeosSubscriber Class
# ============================================================================

class DeosSubscriber:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200, source_node=DEOS_NODE_GROUND_CONTROL):
        self.port = port
        self.baudrate = baudrate
        self.source_node = source_node
        self.seq = 0
        self.running = False
        self.ser = None
        self.rx_thread_handle = None
        
        # Aktif abonelik kayıtları: (node_id, service, command) -> (interval_ms, lease_ttl_s)
        self.active_subscriptions = {}
        self.lock = threading.Lock()

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            tprint(f"Seri port açıldı: {self.port} @ {self.baudrate} baud")
        except Exception as e:
            tprint(f"Seri porta bağlanılamadı: {e}")
            sys.exit(1)

        self.running = True
        self.rx_thread_handle = threading.Thread(target=self._rx_thread, daemon=True)
        self.rx_thread_handle.start()

    def disconnect(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
            tprint("Seri port kapatıldı.")

    def send_lora_frame(self, flag, payload_bytes):
        if not self.ser or not self.ser.is_open:
            return
        length = len(payload_bytes)
        crc = crc16_ccitt(0xFFFF, struct.pack('<B', flag))
        crc = crc16_ccitt(crc, struct.pack('<B', length))
        if length > 0:
            crc = crc16_ccitt(crc, payload_bytes)

        header = struct.pack('<BBBB', LORA_MAGIC_1, LORA_MAGIC_2, flag, length)
        footer = struct.pack('<H', crc)
        frame = header + payload_bytes + footer
        
        self.ser.write(frame)
        self.ser.flush()

    def send_message(self, dest, prio, msg_class, service, cmd, payload=b''):
        # deos_message_t 8-bayt header:
        # priority(1), class(1), service(1), dest(1), source(1), version(1), seq(1), cmd(1)
        header = struct.pack('<BBBBBBBB',
            prio, msg_class, service, dest, self.source_node, 0x11, self.seq, cmd
        )
        self.seq = (self.seq + 1) & 0xFF
        full_payload = header + payload
        self.send_lora_frame(LORA_FLAG_NONE, full_payload)

    def subscribe(self, dest_node, target_service, target_command, interval_ms=1000, lease_ttl_s=0):
        """
        ICD v1.1 Subscribe Mesajı Gönder:
        - dest_node: Hedef üretici node (Örn: 0x03 STM32, 0x20 BMS)
        - target_service: İlgili servis (DEOS_SERVICE_SYSTEM, BMS, vb.)
        - target_command: İlgili komut (HEARTBEAT, BMS_GET_STATUS, vb.)
        - interval_ms: İstenen periyot (ms). 0xFFFF = On-Change, 0 = İptal
        - lease_ttl_s: Kira süresi (saniye). 0 = Sonsuz (Manuel iptale kadar)
        """
        payload = struct.pack('<BBHH', target_service, target_command, interval_ms, lease_ttl_s)
        
        svc_name = SERVICE_NAMES.get(target_service, f"0x{target_service:02X}")
        node_name = NODE_NAMES.get(dest_node, f"0x{dest_node:02X}")
        mode_str = "ON-CHANGE" if interval_ms == DEOS_INTERVAL_ON_CHANGE else f"{interval_ms} ms"
        ttl_str = "SONSUZ" if lease_ttl_s == 0 else f"{lease_ttl_s} s"
        
        tprint(f"[TX] SUBSCRIBE -> Hedef: {node_name} | Servis: {svc_name} (0x{target_service:02X}), "
               f"Komut: 0x{target_command:02X} | Periyot: {mode_str}, TTL: {ttl_str}")
        
        self.send_message(dest_node, DEOS_PRIO_CONTROL, DEOS_CLASS_REQUEST,
                          DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_SUBSCRIBE, payload)
        
        with self.lock:
            self.active_subscriptions[(dest_node, target_service, target_command)] = (interval_ms, lease_ttl_s)

    def unsubscribe(self, dest_node, target_service=0xFF, target_command=0xFF):
        """
        ICD v1.1 Unsubscribe Mesajı Gönder:
        - target_service: 0xFF ise tüm servisler
        - target_command: 0xFF ise tüm komutlar
        """
        payload = struct.pack('<BB', target_service, target_command)
        node_name = NODE_NAMES.get(dest_node, f"0x{dest_node:02X}")
        
        tprint(f"[TX] UNSUBSCRIBE -> Hedef: {node_name} | Servis: 0x{target_service:02X}, Komut: 0x{target_command:02X}")
        self.send_message(dest_node, DEOS_PRIO_CONTROL, DEOS_CLASS_REQUEST,
                          DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_UNSUBSCRIBE, payload)

        with self.lock:
            if target_service == 0xFF:
                keys_to_remove = [k for k in self.active_subscriptions if k[0] == dest_node]
            elif target_command == 0xFF:
                keys_to_remove = [k for k in self.active_subscriptions if k[0] == dest_node and k[1] == target_service]
            else:
                keys_to_remove = [k for k in self.active_subscriptions if k == (dest_node, target_service, target_command)]
            for k in keys_to_remove:
                del self.active_subscriptions[k]

    def unsubscribe_all(self):
        """Tüm kayıtlı abonelikleri kapatır."""
        with self.lock:
            nodes = set(k[0] for k in self.active_subscriptions.keys())
        for node in nodes:
            self.unsubscribe(node, 0xFF, 0xFF)
            time.sleep(0.05)

    def _rx_thread(self):
        state = 0
        rx_flag = 0
        rx_len = 0
        rx_buf = bytearray()

        while self.running:
            try:
                c = self.ser.read(1)
            except Exception:
                break

            if not c:
                continue
            b = c[0]

            if state == 0:
                if b == LORA_MAGIC_1: state = 1
            elif state == 1:
                if b == LORA_MAGIC_2: state = 2
                elif b != LORA_MAGIC_1: state = 0
            elif state == 2:
                rx_flag = b
                state = 3
            elif state == 3:
                rx_len = b
                rx_buf = bytearray()
                if rx_len > 100: state = 0
                elif rx_len == 0: state = 5
                else: state = 4
            elif state == 4:
                rx_buf.append(b)
                if len(rx_buf) >= rx_len: state = 5
            elif state == 5:
                crc_lo = b
                state = 6
            elif state == 6:
                crc_hi = b
                rx_crc = crc_lo | (crc_hi << 8)

                calc = crc16_ccitt(0xFFFF, struct.pack('<B', rx_flag))
                calc = crc16_ccitt(calc, struct.pack('<B', rx_len))
                if rx_len > 0:
                    calc = crc16_ccitt(calc, bytes(rx_buf))

                if calc == rx_crc:
                    self._handle_packet(rx_flag, bytes(rx_buf))
                else:
                    tprint(f"[!] CRC Hatası! Hesaplanan: {calc:04X} != Gelen: {rx_crc:04X}")
                state = 0

    def _handle_packet(self, flag, payload):
        if flag == LORA_FLAG_IS_ACK:
            tprint("[+] ACK alındı.")
            return

        if len(payload) < 8:
            return

        prio, msg_class, service, dest, src, ver, seq, cmd = struct.unpack('<BBBBBBBB', payload[:8])
        msg_payload = payload[8:]
        node_name = NODE_NAMES.get(src, f"0x{src:02X}")

        # 1. SUBSCRIBE RESPONSE
        if service == DEOS_SERVICE_SYSTEM and cmd == DEOS_CMD_SYSTEM_SUBSCRIBE:
            if msg_class == DEOS_CLASS_RESPONSE and len(msg_payload) >= 5:
                res, s_svc, s_cmd, actual_interval = struct.unpack('<BBBH', msg_payload[:5])
                res_name = RESULT_CODES.get(res, f"UNKNOWN(0x{res:02X})")
                svc_str = SERVICE_NAMES.get(s_svc, f"0x{s_svc:02X}")
                tprint(f"[RX] SUBSCRIBE YANITI ({node_name}): Sonuç = {res_name} (0x{res:02X}), "
                       f"Servis: {svc_str}, Komut: 0x{s_cmd:02X}, Tahsis Edilen Periyot: {actual_interval} ms")
            elif msg_class == DEOS_CLASS_RESPONSE and len(msg_payload) >= 1:
                res = msg_payload[0]
                res_name = RESULT_CODES.get(res, f"UNKNOWN(0x{res:02X})")
                tprint(f"[RX] SUBSCRIBE YANITI ({node_name}): Sonuç = {res_name} (0x{res:02X})")

        # 2. UNSUBSCRIBE RESPONSE
        elif service == DEOS_SERVICE_SYSTEM and cmd == DEOS_CMD_SYSTEM_UNSUBSCRIBE:
            if msg_class == DEOS_CLASS_RESPONSE and len(msg_payload) >= 1:
                res = msg_payload[0]
                res_name = RESULT_CODES.get(res, f"UNKNOWN(0x{res:02X})")
                tprint(f"[RX] UNSUBSCRIBE YANITI ({node_name}): Sonuç = {res_name} (0x{res:02X})")

        # 3. HEARTBEAT STREAM
        elif service == DEOS_SERVICE_SYSTEM and cmd == DEOS_CMD_SYSTEM_HEARTBEAT:
            if len(msg_payload) >= 1:
                st = msg_payload[0]
                st_name = STATE_NAMES.get(st, f"0x{st:02X}")
                tprint(f"[STREAM] HEARTBEAT ({node_name}) -> State: {st_name}")
            else:
                tprint(f"[STREAM] HEARTBEAT ({node_name}) -> (Boş)")

        # 4. BMS STATUS STREAM
        elif service == DEOS_SERVICE_BMS and (cmd == DEOS_CMD_BMS_STATUS or cmd == DEOS_CMD_BMS_GET_STATUS):
            if len(msg_payload) >= 16:
                pack_v, pack_i, soc, temp, min_cv, max_cv, nom_v, cell_c, temp_c = struct.unpack('<HhHhHHHBB', msg_payload[:16])
                tprint(f"[STREAM] BMS STATUS ({node_name}): Gerilim: {pack_v/1000.0:.2f} V, Akım: {pack_i/100.0:.2f} A, "
                       f"SOC: {soc/10.0:.1f}%, Sıcaklık: {temp/10.0:.1f} °C, Min/Max: {min_cv}/{max_cv} mV, Hücre Sayısı: {cell_c}")
            else:
                tprint(f"[STREAM] BMS STATUS ({node_name}): Ham veri: {msg_payload.hex()}")

        # 5. BMS CELL VOLTAGES STREAM
        elif service == DEOS_SERVICE_BMS and (cmd == DEOS_CMD_BMS_CELL_VOLTAGES or cmd == DEOS_CMD_BMS_GET_CELL_VOLTAGES):
            if len(msg_payload) >= 3:
                idx, vol = struct.unpack('<BH', msg_payload[:3])
                if idx == 0:
                    tprint(f"[STREAM] BMS CELL VOLTAGES ({node_name}): Hücre Listesi Sonu.")
                else:
                    tprint(f"[STREAM] BMS CELL VOLTAGES ({node_name}): Hücre #{idx} = {vol} mV")

        # 6. LIGHTING STATUS STREAM
        elif service == DEOS_SERVICE_LIGHTING and (cmd == DEOS_CMD_LIGHTING_STATUS or cmd == DEOS_CMD_LIGHTING_GET_STATUS):
            tprint(f"[STREAM] LIGHTING STATUS ({node_name}): Ham: {msg_payload.hex()}")

        # 7. DİĞER TELEMETRİ / YANIT
        else:
            hex_data = " ".join(f"{x:02X}" for x in msg_payload)
            svc_str = SERVICE_NAMES.get(service, f"0x{service:02X}")
            tprint(f"[RX] Servis: {svc_str}, Komut: 0x{cmd:02X}, Sınıf: 0x{msg_class:02X} Kaynak: {node_name} | Veri: [{hex_data}]")

# ============================================================================
# Interactive CLI Menu
# ============================================================================

def interactive_menu(sub):
    print("\n" + "="*70)
    print("          DEOS TELEMETRİ ABONELİK YÖNETİCİSİ (SUBSCRIBER)           ")
    print("="*70)
    print("Hızlı Ön Tanımlı Abonelikler:")
    print("  1) Ana Beyin (STM32 - 0x03) HEARTBEAT Aboneliği (1000 ms, TTL=0)")
    print("  2) Ana Batarya (BMS_MAIN - 0x20) HEARTBEAT Aboneliği (1000 ms, TTL=0)")
    print("  3) Motor/Çekiş (TRACTION - 0x10) HEARTBEAT Aboneliği (1000 ms, TTL=0)")
    print("  4) Direksiyon (STEERING - 0x11) HEARTBEAT Aboneliği (1000 ms, TTL=0)")
    print("  5) Fren (BRAKE - 0x12) HEARTBEAT Aboneliği (1000 ms, TTL=0)")
    print("  6) Ana Batarya (BMS_MAIN - 0x20) STATUS Aboneliği (1000 ms, TTL=0)")
    print("  7) Yedek Batarya (BMS_AUX - 0x21) STATUS Aboneliği (1000 ms, TTL=0)")
    print("  8) Ana Beyin (STM32 - 0x03) On-Change (Değişimde) Aboneliği (0xFFFF)")
    print("-" * 70)
    print("Özel İşlemler:")
    print("  c) Manuel Özel Abonelik Aç (Custom)")
    print("  u) Belirli bir düğümden abonelik iptal et (UNSUBSCRIBE)")
    print("  a) Tüm aktif abonelikleri iptal et")
    print("  l) Aktif yerel abonelikleri listele")
    print("  q) Çıkış (ve tüm abonelikleri temizle)")
    print("="*70 + "\n")

    while True:
        try:
            choice = input("\nSeçiminiz (1-8, c, u, a, l, q): ").strip().lower()
            if not choice:
                continue

            if choice == '1':
                sub.subscribe(DEOS_NODE_MAIN_STM32, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, 1000, 0)
            elif choice == '2':
                sub.subscribe(DEOS_NODE_BMS_MAIN, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, 1000, 0)
            elif choice == '3':
                sub.subscribe(DEOS_NODE_TRACTION, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, 1000, 0)
            elif choice == '4':
                sub.subscribe(DEOS_NODE_STEERING, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, 1000, 0)
            elif choice == '5':
                sub.subscribe(DEOS_NODE_BRAKE, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, 1000, 0)
            elif choice == '6':
                sub.subscribe(DEOS_NODE_BMS_MAIN, DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS, 1000, 0)
            elif choice == '7':
                sub.subscribe(DEOS_NODE_BMS_AUX, DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS, 1000, 0)
            elif choice == '8':
                sub.subscribe(DEOS_NODE_MAIN_STM32, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, DEOS_INTERVAL_ON_CHANGE, 0)
            elif choice == 'c':
                node_hex = input("Hedef Düğüm ID (Hex, örn: 03 veya 20): ").strip()
                dest = int(node_hex, 16)
                svc_hex = input("Hedef Servis ID (Hex, örn: 00 SYSTEM, 04 BMS): ").strip()
                svc = int(svc_hex, 16)
                cmd_hex = input("Hedef Komut ID (Hex, örn: 07 HEARTBEAT, 01 STATUS): ").strip()
                cmd = int(cmd_hex, 16)
                interval_str = input("Periyot ms (Örn: 1000 veya 'c' On-Change): ").strip().lower()
                interval = DEOS_INTERVAL_ON_CHANGE if interval_str == 'c' else int(interval_str)
                ttl_str = input("Lease TTL saniye (0 = Sonsuz): ").strip()
                ttl = int(ttl_str) if ttl_str else 0
                sub.subscribe(dest, svc, cmd, interval, ttl)
            elif choice == 'u':
                node_hex = input("İptal Edilecek Düğüm ID (Hex, örn: 03): ").strip()
                dest = int(node_hex, 16)
                sub.unsubscribe(dest, 0xFF, 0xFF)
            elif choice == 'a':
                sub.unsubscribe_all()
                tprint("Tüm abonelikler iptal edildi.")
            elif choice == 'l':
                with sub.lock:
                    if not sub.active_subscriptions:
                        print("Aktif kayıtlı abonelik bulunmuyor.")
                    else:
                        print("\nAktif Abonelikler:")
                        for (dest, svc, cmd), (interval, ttl) in sub.active_subscriptions.items():
                            n_name = NODE_NAMES.get(dest, f"0x{dest:02X}")
                            s_name = SERVICE_NAMES.get(svc, f"0x{svc:02X}")
                            int_s = "ON-CHANGE" if interval == DEOS_INTERVAL_ON_CHANGE else f"{interval} ms"
                            print(f"  - Düğüm: {n_name} | Servis: {s_name} | Komut: 0x{cmd:02X} | Periyot: {int_s} | TTL: {ttl}s")
            elif choice == 'q':
                tprint("Çıkılıyor, abonelikler sonlandırılıyor...")
                sub.unsubscribe_all()
                time.sleep(0.2)
                break
        except (ValueError, KeyboardInterrupt) as e:
            tprint(f"Girdi iptal edildi: {e}")
            break

# ============================================================================
# Main Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="DEOS ICD v1.1 Telemetri Abonelik Aracı")
    parser.add_argument("-p", "--port", default="/dev/ttyUSB0", help="Seri port (Varsayılan: /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baudrate (Varsayılan: 115200)")
    parser.add_argument("--node", type=lambda x: int(x, 0), help="Hedef Düğüm ID (Hex veya int, örn: 0x03)")
    parser.add_argument("--service", type=lambda x: int(x, 0), default=DEOS_SERVICE_SYSTEM, help="Hedef Servis ID (Varsayılan: 0x00 SYSTEM)")
    parser.add_argument("--command", type=lambda x: int(x, 0), default=DEOS_CMD_SYSTEM_HEARTBEAT, help="Hedef Komut ID (Varsayılan: 0x07 HEARTBEAT)")
    parser.add_argument("--interval", type=int, default=1000, help="Periyot ms (Varsayılan: 1000 ms, On-Change için 65535)")
    parser.add_argument("--ttl", type=int, default=0, help="Lease TTL süresi (Varsayılan: 0 sonsuz)")
    parser.add_argument("--unsub", action="store_true", help="Abonelik açmak yerine iptal et")
    
    args = parser.parse_args()

    sub = DeosSubscriber(port=args.port, baudrate=args.baud)
    sub.connect()

    try:
        if args.node is not None:
            if args.unsub:
                sub.unsubscribe(args.node, args.service, args.command)
            else:
                sub.subscribe(args.node, args.service, args.command, args.interval, args.ttl)
            tprint("Paketler dinleniyor... Çıkmak için Ctrl+C'ye basın.")
            while True:
                time.sleep(1)
        else:
            interactive_menu(sub)
    except KeyboardInterrupt:
        tprint("\nSonlandırılıyor...")
    finally:
        sub.disconnect()

if __name__ == '__main__':
    main()

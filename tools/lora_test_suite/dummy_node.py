import serial
import struct
import time
import threading
import sys
from datetime import datetime

def tprint(*args, **kwargs):
    msg = " ".join(map(str, args))
    print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {msg}", **kwargs)

# DEOS Node IDs
DEOS_NODE_GROUND_CONTROL = 0x02
DEOS_NODE_MAIN_STM32 = 0x03
DEOS_NODE_TRACTION = 0x10
DEOS_NODE_STEERING = 0x11
DEOS_NODE_BRAKE = 0x12
DEOS_NODE_BMS_MAIN = 0x20
DEOS_NODE_BMS_AUX = 0x21
DEOS_NODE_BMS_GATEWAY = 0x30

# DEOS Priorities
DEOS_PRIO_CONTROL = 2
DEOS_PRIO_STATUS = 3
DEOS_PRIO_CONFIG = 4
DEOS_PRIO_NETWORK = 5

# DEOS Classes
DEOS_CLASS_SAFETY   = 0x00
DEOS_CLASS_COMMAND  = 0x01
DEOS_CLASS_STATUS   = 0x02
DEOS_CLASS_EVENT    = 0x03
DEOS_CLASS_REQUEST  = 0x04
DEOS_CLASS_RESPONSE = 0x05
DEOS_CLASS_CONFIG   = 0x06
DEOS_CLASS_NETWORK  = 0x07

# DEOS Services
DEOS_SERVICE_SYSTEM = 0x00
DEOS_SERVICE_TRACTION = 0x01
DEOS_SERVICE_STEERING = 0x02
DEOS_SERVICE_BRAKE = 0x03
DEOS_SERVICE_BMS = 0x04
DEOS_SERVICE_SAFETY = 0x05
DEOS_SERVICE_DIAGNOSTIC = 0x06
DEOS_SERVICE_SENSORS = 0x07
DEOS_SERVICE_LIGHTING = 0x08

# DEOS Commands
DEOS_CMD_SYSTEM_GET_STATE = 0x01
DEOS_CMD_SYSTEM_SET_STATE = 0x02
DEOS_CMD_SYSTEM_HEARTBEAT = 0x07
DEOS_CMD_SYSTEM_PING = 0x08
DEOS_CMD_SYSTEM_PONG = 0x09
DEOS_CMD_SYSTEM_SUBSCRIBE = 0x0A
DEOS_CMD_SYSTEM_UNSUBSCRIBE = 0x0B
DEOS_CMD_SYSTEM_GET_SUBSCRIPTIONS = 0x0C

DEOS_CMD_BMS_GET_STATUS = 0x01
DEOS_CMD_BMS_STATUS = 0x02
DEOS_CMD_BMS_GET_CELL_VOLTAGES = 0x03
DEOS_CMD_BMS_CELL_VOLTAGES = 0x04
DEOS_CMD_BMS_GET_BALANCING = 0x07
DEOS_CMD_BMS_BALANCING = 0x08

DEOS_INTERVAL_ON_CHANGE = 0xFFFF

# DEOS Result Codes
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

# State Definitions
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

# UART LORA Protocol Constants
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

class DeosLoraDummy:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            tprint(f"Baudrate {baudrate} ile {port} portuna bağlanıldı.")
        except Exception as e:
            tprint(f"Seri port hatası: {e}")
            sys.exit(1)
            
        self.seq = 0
        self.running = True

    def send_lora_frame(self, flag, payload_bytes):
        length = len(payload_bytes)
        # CRC hesaplaması Zephyr crc16_ccitt (0xFFFF seed) ile birebir aynı
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
        # Universal DEOS Transport Frame (9-bayt header):
        # dest(1), src(1), prio(1), ver(1=0x11), class(1), service(1), cmd(1), seq(1), len(1)
        header = struct.pack('<BBBBBBBBB',
            dest, DEOS_NODE_GROUND_CONTROL, prio, 0x11, msg_class, service, cmd, self.seq, len(payload)
        )
        self.seq = (self.seq + 1) & 0xFF
        full_payload = header + payload
        self.send_lora_frame(LORA_FLAG_NONE, full_payload)

    def subscribe(self, dest_node, target_service, target_command, interval_ms=1000, lease_ttl_s=0):
        """
        ICD v1.1 Telemetri/Durum Aboneliği (SUBSCRIBE):
        - target_service: Abone olunmak istenen servis (DEOS_SERVICE_SYSTEM, BMS, vb.)
        - target_command: Abone olunmak istenen komut (HEARTBEAT, BMS_GET_STATUS, vb.)
        - interval_ms: İstenen periyot (0=iptal, 10-65534=ms cinsinden periyot, 0xFFFF=On-Change)
        - lease_ttl_s: Kira süresi (saniye). 0 = Sonsuz.
        """
        payload = struct.pack('<BBHH', target_service, target_command, interval_ms, lease_ttl_s)
        tprint(f"[TX] SUBSCRIBE -> Node 0x{dest_node:02X} | Svc: 0x{target_service:02X}, Cmd: 0x{target_command:02X}, Interval: {interval_ms} ms, TTL: {lease_ttl_s} s")
        self.send_message(dest_node, DEOS_PRIO_CONTROL, DEOS_CLASS_REQUEST, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_SUBSCRIBE, payload)

    def unsubscribe(self, dest_node, target_service=0xFF, target_command=0xFF):
        """
        ICD v1.1 Abonelik İptali (UNSUBSCRIBE):
        - target_service: 0xFF ise o node'daki tüm servisler
        - target_command: 0xFF ise servise ait tüm komutlar
        """
        payload = struct.pack('<BB', target_service, target_command)
        tprint(f"[TX] UNSUBSCRIBE -> Node 0x{dest_node:02X} | Svc: 0x{target_service:02X}, Cmd: 0x{target_command:02X}")
        self.send_message(dest_node, DEOS_PRIO_CONTROL, DEOS_CLASS_REQUEST, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_UNSUBSCRIBE, payload)

    def rx_thread(self):
        state = 0
        rx_flag = 0
        rx_len = 0
        rx_buf = bytearray()
        
        while self.running:
            try:
                c = self.ser.read(1)
            except:
                break
                
            if not c:
                continue
            b = c[0]
            
            # LoRa UART State Machine
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
                
                # Gelen verinin CRC'sini doğrula
                calc = crc16_ccitt(0xFFFF, struct.pack('<B', rx_flag))
                calc = crc16_ccitt(calc, struct.pack('<B', rx_len))
                if rx_len > 0:
                    calc = crc16_ccitt(calc, bytes(rx_buf))
                    
                if calc == rx_crc:
                    self.handle_packet(rx_flag, bytes(rx_buf))
                else:
                    tprint(f"[!] CRC Hatası! Hesaplanan: {calc:04X} != Gelen: {rx_crc:04X}")
                state = 0

    def handle_packet(self, flag, payload):
        if flag == LORA_FLAG_IS_ACK:
            tprint("[+] ACK alındı.")
            return
            
        if len(payload) < 9:
            return
            
        dest, src, prio, ver, msg_class, service, cmd, seq, payload_len = struct.unpack('<BBBBBBBBB', payload[:9])
        msg_payload = payload[9:9+payload_len]
        
        # PONG
        if service == DEOS_SERVICE_SYSTEM and cmd == DEOS_CMD_SYSTEM_PONG:
            tprint(f"[RX] PONG (Node 0x{src:02X})")
            
        # HEARTBEAT
        elif service == DEOS_SERVICE_SYSTEM and cmd == DEOS_CMD_SYSTEM_HEARTBEAT:
            if len(msg_payload) >= 1:
                st = msg_payload[0]
                st_name = STATE_NAMES.get(st, f"0x{st:02X}")
                tprint(f"[RX] HEARTBEAT (Node 0x{src:02X}) -> State: {st_name}")
            else:
                tprint(f"[RX] HEARTBEAT (Node 0x{src:02X}) -> (Payload yok)")

        # SUBSCRIBE RESPONSE
        elif service == DEOS_SERVICE_SYSTEM and cmd == DEOS_CMD_SYSTEM_SUBSCRIBE:
            if msg_class == DEOS_CLASS_RESPONSE and len(msg_payload) >= 5:
                res, s_svc, s_cmd, actual_interval = struct.unpack('<BBBH', msg_payload[:5])
                res_name = RESULT_CODES.get(res, f"UNKNOWN(0x{res:02X})")
                tprint(f"[RX] SUBSCRIBE YANITI (Node 0x{src:02X}): Sonuc = {res_name}, Svc: 0x{s_svc:02X}, Cmd: 0x{s_cmd:02X}, Interval: {actual_interval} ms")
            elif msg_class == DEOS_CLASS_RESPONSE and len(msg_payload) >= 1:
                res = msg_payload[0]
                res_name = RESULT_CODES.get(res, f"UNKNOWN(0x{res:02X})")
                tprint(f"[RX] SUBSCRIBE YANITI (Node 0x{src:02X}): Sonuc = {res_name}")

        # UNSUBSCRIBE RESPONSE
        elif service == DEOS_SERVICE_SYSTEM and cmd == DEOS_CMD_SYSTEM_UNSUBSCRIBE:
            if msg_class == DEOS_CLASS_RESPONSE and len(msg_payload) >= 1:
                res = msg_payload[0]
                res_name = RESULT_CODES.get(res, f"UNKNOWN(0x{res:02X})")
                tprint(f"[RX] UNSUBSCRIBE YANITI (Node 0x{src:02X}): Sonuc = {res_name}")

        # BMS STATUS
        elif service == DEOS_SERVICE_BMS and (cmd == DEOS_CMD_BMS_STATUS or cmd == DEOS_CMD_BMS_GET_STATUS):
            if len(msg_payload) >= 16:
                pack_v, pack_i, soc, temp, min_cv, max_cv, nom_v, cell_c, temp_c = struct.unpack('<HhHhHHHBB', msg_payload[:16])
                tprint(f"[RX] BMS STATUS (Node 0x{src:02X}): Pack V: {pack_v} mV, I: {pack_i} cA, SOC: {soc/10} %, Cells: {cell_c}")
                
        # BMS CELL VOLTAGE
        elif service == DEOS_SERVICE_BMS and (cmd == DEOS_CMD_BMS_CELL_VOLTAGES or cmd == DEOS_CMD_BMS_GET_CELL_VOLTAGES):
            if len(msg_payload) >= 3:
                idx, vol = struct.unpack('<BH', msg_payload[:3])
                if idx == 0:
                    tprint(f"[RX] BMS CELL VOLTAGE (Node 0x{src:02X}): Liste Sonu.")
                else:
                    tprint(f"[RX] BMS CELL VOLTAGE (Node 0x{src:02X}): Hücre {idx} = {vol} mV")
                    
        # BMS BALANCING
        elif service == DEOS_SERVICE_BMS and (cmd == DEOS_CMD_BMS_BALANCING or cmd == DEOS_CMD_BMS_GET_BALANCING):
            if len(msg_payload) >= 2:
                idx, st = struct.unpack('<BB', msg_payload[:2])
                if idx == 0:
                    tprint(f"[RX] BMS BALANCING (Node 0x{src:02X}): Liste Sonu. Genel Durum = {st}")
                else:
                    tprint(f"[RX] BMS BALANCING (Node 0x{src:02X}): Hücre {idx} Durumu = {st}")
        else:
            tprint(f"[RX] Servis: 0x{service:02X}, Cmd: 0x{cmd:02X} (Gelen: 0x{src:02X})")

    def run(self):
        t = threading.Thread(target=self.rx_thread, daemon=True)
        t.start()
        
        try:
            tprint("--- LoRa Dummy Diagnostic Tool Başlatıldı ---")
            tprint("Ana Düğüme (Main STM32) HEARTBEAT aboneliği başlatılıyor (1000 ms, TTL=0)...")
            self.subscribe(DEOS_NODE_MAIN_STM32, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, interval_ms=1000, lease_ttl_s=0)
            time.sleep(0.5)

            tprint("BMS Düğümüne (BMS Main) GET_STATUS aboneliği başlatılıyor (1000 ms, TTL=0)...")
            self.subscribe(DEOS_NODE_BMS_MAIN, DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS, interval_ms=1000, lease_ttl_s=0)
            
            while True:
                tprint("\n[TX] Periyodik PING atılıyor...")
                ping_payload = struct.pack('<I', 0xDEADBEEF)
                self.send_message(DEOS_NODE_MAIN_STM32, DEOS_PRIO_NETWORK, DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_PING, ping_payload)
                
                tprint("Abonelik akışı ve ping yanıtları dinleniyor (3 saniye)...")
                time.sleep(3)
                
        except KeyboardInterrupt:
            tprint("\nÇıkış yapılıyor, abonelikler iptal ediliyor...")
            try:
                self.unsubscribe(DEOS_NODE_MAIN_STM32, 0xFF, 0xFF)
                time.sleep(0.1)
                self.unsubscribe(DEOS_NODE_BMS_MAIN, 0xFF, 0xFF)
                time.sleep(0.2)
            except:
                pass
            self.running = False
            self.ser.close()

if __name__ == '__main__':
    # Seri portunuzu buraya yazın (Örn: /dev/ttyUSB0 veya COM3)
    PORT = '/dev/ttyUSB0' 
    BAUDRATE = 115200 # STM32 115200'de dinliyor!
    
    dummy = DeosLoraDummy(port=PORT, baudrate=BAUDRATE)
    dummy.run()

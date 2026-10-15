import serial
import struct
import time
import sys
import threading
from datetime import datetime

def tprint(*args, **kwargs):
    msg = " ".join(map(str, args))
    print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {msg}", **kwargs)

LORA_MAGIC_1 = 0xAA
LORA_MAGIC_2 = 0x55
LORA_FLAG_NONE = 0x00
LORA_FLAG_IS_ACK = 0x02

DEOS_PRIO_NETWORK = 5
DEOS_CLASS_NETWORK = 0x07
DEOS_SERVICE_SYSTEM = 0x00
DEOS_CMD_SYSTEM_PING = 0x08
DEOS_CMD_SYSTEM_PONG = 0x09
DEOS_NODE_GROUND_CONTROL = 0x02

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

class PongResponder:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            tprint(f"Baudrate {baudrate} ile {port} portuna baglanildi.")
            tprint("DEOS Pong Responder Basladi. PING mesajlarina otomatik PONG donecek.")
        except Exception as e:
            tprint(f"Seri port hatasi: {e}")
            sys.exit(1)
        self.seq = 0
        self.running = True

    def send_lora_frame(self, flag, payload_bytes):
        length = len(payload_bytes)
        crc = crc16_ccitt(0xFFFF, struct.pack('<B', flag))
        crc = crc16_ccitt(crc, struct.pack('<B', length))
        if length > 0:
            crc = crc16_ccitt(crc, payload_bytes)
            
        header = struct.pack('<BBBB', LORA_MAGIC_1, LORA_MAGIC_2, flag, length)
        footer = struct.pack('<H', crc)
        self.ser.write(header + payload_bytes + footer)
        self.ser.flush()

    def send_pong(self, dest_node, ping_id):
        payload = struct.pack('<I', ping_id) # 4 bytes ping id
        header = struct.pack('<BBBBBBBBB',
            dest_node, DEOS_NODE_GROUND_CONTROL, DEOS_PRIO_NETWORK, 0x11,
            DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_PONG,
            self.seq, len(payload)
        )
        self.seq = (self.seq + 1) & 0xFF
        tprint(f"[TX] PONG gonderiliyor -> Hedef: 0x{dest_node:02X}, Ping ID: {ping_id}")
        self.send_lora_frame(LORA_FLAG_NONE, header + payload)

    def rx_thread(self):
        state = 0
        rx_flag = 0
        rx_len = 0
        rx_buf = bytearray()
        
        while self.running:
            c = self.ser.read(1)
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
                    self.handle_packet(rx_flag, bytes(rx_buf))
                state = 0

    def handle_packet(self, flag, payload):
        if len(payload) < 9:
            return
            
        dest, src, prio, ver, msg_class, service, cmd, seq, payload_len = struct.unpack('<BBBBBBBBB', payload[:9])
        msg_payload = payload[9:9+payload_len]
        
        if msg_class == DEOS_CLASS_NETWORK and cmd == DEOS_CMD_SYSTEM_PING:
            if len(msg_payload) == 4:
                recv_ping_id = struct.unpack('<I', msg_payload)[0]
                tprint(f"[RX] PING alindi <- Kaynak: 0x{src:02X}, Ping ID: {recv_ping_id}")
                self.send_pong(src, recv_ping_id)
            else:
                tprint(f"[RX] PING alindi <- Kaynak: 0x{src:02X}, HATA: Payload 4 byte degil!")

    def run(self):
        t = threading.Thread(target=self.rx_thread, daemon=True)
        t.start()
        
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
            
        self.running = False
        self.ser.close()

if __name__ == '__main__':
    app = PongResponder()
    app.run()

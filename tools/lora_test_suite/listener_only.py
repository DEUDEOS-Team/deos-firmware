import serial
import struct
import time
import sys
from datetime import datetime

def tprint(*args, **kwargs):
    msg = " ".join(map(str, args))
    print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {msg}", **kwargs)

LORA_MAGIC_1 = 0xAA
LORA_MAGIC_2 = 0x55
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

class DeosListener:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            tprint(f"Baudrate {baudrate} ile {port} portuna baglanildi.")
            tprint("Sadece gelen paketler dinleniyor. (Cikmak icin Ctrl+C)")
        except Exception as e:
            tprint(f"Seri port hatasi: {e}")
            sys.exit(1)

    def listen(self):
        state = 0
        rx_flag = 0
        rx_len = 0
        rx_buf = bytearray()
        
        try:
            while True:
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
                    else:
                        tprint(f"[CRC ERROR] Calc: {calc:04X}, Rx: {rx_crc:04X}")
                    state = 0
        except KeyboardInterrupt:
            tprint("\nCikis yapiliyor...")
        finally:
            self.ser.close()

    def handle_packet(self, flag, payload):
        if flag == LORA_FLAG_IS_ACK:
            tprint("[ACK RECEIVED]")
            return
            
        if len(payload) < 9:
            return
            
        dest, src, prio, ver, msg_class, service, cmd, seq, payload_len = struct.unpack('<BBBBBBBBB', payload[:9])
        msg_payload = payload[9:9+payload_len]
        
        hex_payload = " ".join([f"{b:02X}" for b in msg_payload])
        tprint(f"[RX] Prio:{prio} Class:{msg_class} Srv:{service} Dst:0x{dest:02X} Src:0x{src:02X} Cmd:0x{cmd:02X} Len:{len(msg_payload)} Data:[{hex_payload}]")

if __name__ == '__main__':
    listener = DeosListener()
    listener.listen()

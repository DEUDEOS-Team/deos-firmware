
import serial
import time

PORT = "/dev/ttyUSB0"
BAUDRATE = 9600

def show(data):
    return " ".join(f"{x:02X}" for x in data)

with serial.Serial(PORT, BAUDRATE, timeout=2) as ser:
    time.sleep(0.2)

    # Read current REG0
    ser.reset_input_buffer()
    ser.write(bytes.fromhex("C1 02 01"))
    old = ser.read(4)

    print("OLD:", show(old))

    if len(old) != 4 or old[:3] != bytes.fromhex("C1 02 01"):
        raise RuntimeError("Register read failed")

    # Change ONLY air data rate bits (2:0)
    new_reg0 = (old[3] & 0xF8) | 0x07

    # Permanently write REG0
    command = bytes([0xC0, 0x02, 0x01, new_reg0])

    ser.reset_input_buffer()
    ser.write(command)
    ser.flush()

    response = ser.read(4)

    print("TX :", show(command))
    print("RX :", show(response))

    if response != bytes([0xC1, 0x02, 0x01, new_reg0]):
        raise RuntimeError("Write verification failed")

    # Read back
    ser.reset_input_buffer()
    ser.write(bytes.fromhex("C1 02 01"))
    result = ser.read(4)

    print("VERIFY:", show(result))

    if result == bytes([0xC1, 0x02, 0x01, new_reg0]):
        print("SUCCESS: Air Data Rate = 62.5 kbps")
    else:
        print("ERROR: Register verification failed")


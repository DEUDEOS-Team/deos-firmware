
#!/usr/bin/env python3

import serial
import time

PORT = "/dev/ttyUSB0"
BAUDRATE = 9600

REGISTERS = {
    0x00: "ADDH",
    0x01: "ADDL",
    0x02: "REG0",
    0x03: "REG1",
    0x04: "REG2 (Channel)",
    0x05: "REG3",
    0x06: "CRYPT_H",
    0x07: "CRYPT_L",
}

def hex_string(data):
    return " ".join(f"{b:02X}" for b in data)

def read_registers(ser, start=0x00, count=0x08):
    command = bytes([0xC1, start, count])

    ser.reset_input_buffer()
    ser.write(command)
    ser.flush()

    response = ser.read(3 + count)

    print("TX:", hex_string(command))
    print("RX:", hex_string(response))

    if len(response) != 3 + count:
        raise RuntimeError("Eksik yanit veya timeout")

    if response[:3] != command:
        raise RuntimeError("Gecersiz register yaniti")

    return response[3:]


def main():
    with serial.Serial(
        port=PORT,
        baudrate=BAUDRATE,
        bytesize=8,
        parity=serial.PARITY_NONE,
        stopbits=1,
        timeout=2
    ) as ser:

        time.sleep(0.2)

        data = read_registers(ser)

        print("\n========== E220 REGISTERS ==========")

        for address, value in enumerate(data):
            name = REGISTERS.get(address, "UNKNOWN")
            print(
                f"0x{address:02X} | "
                f"{name:18s} | "
                f"0x{value:02X} | "
                f"{value:08b}"
            )

        module_address = (data[0] << 8) | data[1]

        print("\n========== SUMMARY ==========")
        print(f"Module Address : 0x{module_address:04X}")
        print(f"Channel        : {data[4]}")
        print(f"REG0           : 0x{data[2]:02X}")
        print(f"REG1           : 0x{data[3]:02X}")
        print(f"REG3           : 0x{data[5]:02X}")


if __name__ == "__main__":
    main()

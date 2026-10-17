"""
DEOS Protocol Python C-Bindings (ctypes wrapper)
libdeos_protocol C kütüphanesini doğrudan çağıran resmi Python bağlayıcısıdır.
"""

import os
import sys
import ctypes
import subprocess
from pathlib import Path
from typing import Callable, Optional, Dict, List, Any

# ==============================================================================
# 1. Kütüphane Yükleyici (Shared Library Loader)
# ==============================================================================

def _find_or_build_library() -> ctypes.CDLL:
    current_dir = Path(__file__).resolve().parent
    repo_root = current_dir.parent.parent.parent

    search_paths = [
        current_dir / "libdeos_protocol.so",
        repo_root / "tests/desktop/build/deos_protocol_build/libdeos_protocol.so",
        repo_root / "build/lib/libdeos_protocol.so",
        Path("/tmp/libdeos_protocol.so"),
    ]

    for p in search_paths:
        if p.exists():
            try:
                return ctypes.CDLL(str(p))
            except Exception:
                pass

    # Eğer .so bulunamazsa C kaynak kodlarından otomatik derle
    c_sources = [
        str(current_dir.parent / "src/deos_frame.c"),
        str(current_dir.parent / "src/deos_codec.c"),
        str(current_dir.parent / "src/deos_registry.c"),
    ]
    include_dir = str(current_dir.parent / "include")
    out_so = "/tmp/libdeos_protocol.so"

    cmd = ["gcc", "-shared", "-fPIC", "-O2", "-o", out_so] + c_sources + [f"-I{include_dir}"]
    try:
        subprocess.check_call(cmd)
        return ctypes.CDLL(out_so)
    except Exception as e:
        raise RuntimeError(f"libdeos_protocol.so derlenemedi veya bulunamadı: {e}")

_lib = _find_or_build_library()

# ==============================================================================
# 2. ICD Sabitleri
# ==============================================================================

DEOS_UNIVERSAL_HEADER_SIZE = 9
DEOS_PROTOCOL_VERSION      = 0x11
DEOS_MAX_PAYLOAD_LEN       = 60

# Öncelik Düzeyleri (Priority)
PRIO_EMERGENCY        = 0
PRIO_CRITICAL_CONTROL = 1
PRIO_CONTROL          = 2
PRIO_STATUS           = 3
PRIO_CONFIG           = 4
PRIO_NETWORK          = 5
PRIO_LOW              = 6
PRIO_BACKGROUND       = 7

# Düğüm ID'leri (Node IDs)
NODE_INVALID          = 0x00
NODE_TEXTUAL          = 0x01
NODE_GROUND_CONTROL   = 0x02
NODE_MAIN_STM32       = 0x03
NODE_ROS_BRIDGE       = 0x04
NODE_TRACTION         = 0x10
NODE_STEERING         = 0x11
NODE_BRAKE            = 0x12
NODE_IMU              = 0x13
NODE_BMS_MAIN         = 0x20
NODE_BMS_AUX          = 0x21
NODE_BMS_GATEWAY      = 0x30
NODE_DIAG_TOOL        = 0xF0
NODE_CAN_BROADCAST    = 0xFE
NODE_GLOBAL_BROADCAST = 0xFF

# Mesaj Sınıfları (Message Class)
CLASS_SAFETY          = 0x00
CLASS_COMMAND         = 0x01
CLASS_STATUS          = 0x02
CLASS_EVENT           = 0x03
CLASS_REQUEST         = 0x04
CLASS_RESPONSE        = 0x05
CLASS_CONFIG          = 0x06
CLASS_NETWORK         = 0x07

# Servis ID'leri (Service IDs)
SERVICE_SYSTEM        = 0x00
SERVICE_TRACTION      = 0x01
SERVICE_STEERING      = 0x02
SERVICE_BRAKE         = 0x03
SERVICE_BMS           = 0x04
SERVICE_CALIBRATION   = 0x05
SERVICE_DIAGNOSTIC    = 0x06
SERVICE_SENSORS       = 0x07
SERVICE_LIGHTING      = 0x08

# Komut Tanımları (Commands)
CMD_SYSTEM_GET_STATE          = 0x01
CMD_SYSTEM_SET_STATE          = 0x02
CMD_SYSTEM_GET_MODE           = 0x03
CMD_SYSTEM_SET_MODE           = 0x04
CMD_SYSTEM_RESET_NODE         = 0x05
CMD_SYSTEM_GET_NODE_INFO      = 0x06
CMD_SYSTEM_HEARTBEAT          = 0x07
CMD_SYSTEM_PING               = 0x08
CMD_SYSTEM_PONG               = 0x09
CMD_SYSTEM_SUBSCRIBE          = 0x0A
CMD_SYSTEM_UNSUBSCRIBE        = 0x0B

CMD_TRACTION_SET_TARGET_SPEED = 0x01
CMD_TRACTION_GET_STATUS       = 0x02
CMD_TRACTION_SET_GEAR         = 0x03
CMD_TRACTION_SET_TORQUE_LIMIT = 0x04

CMD_STEERING_SET_TARGET_ANGLE = 0x01
CMD_STEERING_GET_STATUS       = 0x02

CMD_BRAKE_SET_TARGET          = 0x01
CMD_BRAKE_GET_STATUS          = 0x02


# ==============================================================================
# 3. C ctypes Yapıları ve Fonksiyon İmzaları
# ==============================================================================

class _CDeosMessage(ctypes.Structure):
    _fields_ = [
        ("priority",      ctypes.c_int),
        ("message_class", ctypes.c_int),
        ("service",       ctypes.c_int),
        ("destination",   ctypes.c_int),
        ("source",        ctypes.c_int),
        ("version",       ctypes.c_uint8),
        ("sequence",      ctypes.c_uint8),
        ("command",       ctypes.c_uint8),
        ("payload",       ctypes.c_uint8 * DEOS_MAX_PAYLOAD_LEN),
        ("payload_len",   ctypes.c_uint8),
    ]

_C_HANDLER_TYPE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.POINTER(_CDeosMessage), ctypes.c_void_p)

# Fonksiyon Bağlantıları
_lib.deos_frame_encode.argtypes = [ctypes.POINTER(_CDeosMessage), ctypes.c_char_p, ctypes.c_size_t]
_lib.deos_frame_encode.restype  = ctypes.c_int

_lib.deos_frame_decode.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(_CDeosMessage)]
_lib.deos_frame_decode.restype  = ctypes.c_int

_lib.deos_node_name.argtypes    = [ctypes.c_uint8]
_lib.deos_node_name.restype     = ctypes.c_char_p

_lib.deos_service_name.argtypes = [ctypes.c_uint8]
_lib.deos_service_name.restype  = ctypes.c_char_p

_lib.deos_class_name.argtypes   = [ctypes.c_uint8]
_lib.deos_class_name.restype    = ctypes.c_char_p

_lib.deos_command_name.argtypes = [ctypes.c_uint8, ctypes.c_uint8]
_lib.deos_command_name.restype  = ctypes.c_char_p

_lib.deos_registry_init.argtypes = [ctypes.c_void_p]
_lib.deos_registry_init.restype  = ctypes.c_int

_lib.deos_registry_register_for_node.argtypes = [
    ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, _C_HANDLER_TYPE, ctypes.c_void_p
]
_lib.deos_registry_register_for_node.restype = ctypes.c_int

_lib.deos_registry_dispatch.argtypes = [ctypes.c_void_p, ctypes.POINTER(_CDeosMessage)]
_lib.deos_registry_dispatch.restype  = ctypes.c_int

# Codec Fonksiyonları
_lib.deos_put_u16_le.argtypes = [ctypes.c_char_p, ctypes.c_uint16]
_lib.deos_put_u16_le.restype  = None

_lib.deos_get_u16_le.argtypes = [ctypes.c_char_p]
_lib.deos_get_u16_le.restype  = ctypes.c_uint16

_lib.deos_put_i16_le.argtypes = [ctypes.c_char_p, ctypes.c_int16]
_lib.deos_put_i16_le.restype  = None

_lib.deos_get_i16_le.argtypes = [ctypes.c_char_p]
_lib.deos_get_i16_le.restype  = ctypes.c_int16

_lib.deos_put_u32_le.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
_lib.deos_put_u32_le.restype  = None

_lib.deos_get_u32_le.argtypes = [ctypes.c_char_p]
_lib.deos_get_u32_le.restype  = ctypes.c_uint32

_lib.deos_put_i32_le.argtypes = [ctypes.c_char_p, ctypes.c_int32]
_lib.deos_put_i32_le.restype  = None

_lib.deos_get_i32_le.argtypes = [ctypes.c_char_p]
_lib.deos_get_i32_le.restype  = ctypes.c_int32


# ==============================================================================
# 4. Yüksek Seviye Pythonic Arayüz (Wrapper)
# ==============================================================================

class DeosMessage:
    """
    C kütüphanesindeki `deos_message_t` yapısının yüksek seviyeli Python sarmalayıcısı.
    Tüm encode/decode işlemleri C kütüphanesi (src/deos_frame.c) üzerinden yürütülür.
    """
    def __init__(self,
                 destination: int = NODE_INVALID,
                 source: int = NODE_INVALID,
                 priority: int = PRIO_STATUS,
                 version: int = DEOS_PROTOCOL_VERSION,
                 message_class: int = CLASS_COMMAND,
                 service: int = SERVICE_SYSTEM,
                 command: int = 0,
                 sequence: int = 0,
                 payload: bytes = b""):
        self._c = _CDeosMessage()
        self.destination = destination
        self.source = source
        self.priority = priority
        self.version = version
        self.message_class = message_class
        self.service = service
        self.command = command
        self.sequence = sequence
        self.payload = payload

    @property
    def destination(self) -> int: return self._c.destination
    @destination.setter
    def destination(self, val: int): self._c.destination = int(val)

    @property
    def source(self) -> int: return self._c.source
    @source.setter
    def source(self, val: int): self._c.source = int(val)

    @property
    def priority(self) -> int: return self._c.priority
    @priority.setter
    def priority(self, val: int): self._c.priority = int(val)

    @property
    def version(self) -> int: return self._c.version
    @version.setter
    def version(self, val: int): self._c.version = int(val)

    @property
    def message_class(self) -> int: return self._c.message_class
    @message_class.setter
    def message_class(self, val: int): self._c.message_class = int(val)

    @property
    def service(self) -> int: return self._c.service
    @service.setter
    def service(self, val: int): self._c.service = int(val)

    @property
    def command(self) -> int: return self._c.command
    @command.setter
    def command(self, val: int): self._c.command = int(val)

    @property
    def sequence(self) -> int: return self._c.sequence
    @sequence.setter
    def sequence(self, val: int): self._c.sequence = int(val)

    @property
    def payload(self) -> bytes:
        return bytes(self._c.payload[:self._c.payload_len])

    @payload.setter
    def payload(self, data: bytes):
        if len(data) > DEOS_MAX_PAYLOAD_LEN:
            raise ValueError(f"Payload boyutu izin verilen {DEOS_MAX_PAYLOAD_LEN} bayttan büyük: {len(data)}")
        self._c.payload_len = len(data)
        for i, b in enumerate(data):
            self._c.payload[i] = b

    def encode(self, buf_max: int = 128) -> bytes:
        """
        C fonksiyonu `deos_frame_encode` çağırarak mesajı Universal Frame baytlarına dönüştürür.
        """
        out_buf = ctypes.create_string_buffer(buf_max)
        res = _lib.deos_frame_encode(ctypes.byref(self._c), out_buf, buf_max)
        if res < 0:
            raise RuntimeError(f"deos_frame_encode C hatası döndürdü: {res}")
        return out_buf.raw[:res]

    @classmethod
    def decode(cls, frame_bytes: bytes) -> Optional["DeosMessage"]:
        """
        C fonksiyonu `deos_frame_decode` çağırarak bayt dizisinden DeosMessage nesnesi oluşturur.
        """
        c_msg = _CDeosMessage()
        res = _lib.deos_frame_decode(frame_bytes, len(frame_bytes), ctypes.byref(c_msg))
        if res != 0:
            return None
        msg = cls()
        msg._c = c_msg
        return msg

    @property
    def destination_name(self) -> str:
        return _lib.deos_node_name(self.destination).decode("utf-8")

    @property
    def source_name(self) -> str:
        return _lib.deos_node_name(self.source).decode("utf-8")

    @property
    def service_name(self) -> str:
        return _lib.deos_service_name(self.service).decode("utf-8")

    @property
    def class_name(self) -> str:
        return _lib.deos_class_name(self.message_class).decode("utf-8")

    @property
    def command_name(self) -> str:
        return _lib.deos_command_name(self.service, self.command).decode("utf-8")

    def __repr__(self) -> str:
        return (f"<DeosMessage Src:{self.source_name} -> Dest:{self.destination_name} | "
                f"Class:{self.class_name} Svc:{self.service_name} Cmd:{self.command_name} | "
                f"Len:{len(self.payload)} Payload:{self.payload.hex()}>")


class DeosRegistry:
    """
    C kütüphanesindeki `deos_registry_t` mekanizmasını doğrudan çalıştırır.
    Python callback fonksiyonlarını C fonksiyon işaretçilerine bağlar.
    """
    def __init__(self):
        # sizeof(deos_registry_t) = 1288 bytes
        self._reg_buf = ctypes.create_string_buffer(1300)
        _lib.deos_registry_init(self._reg_buf)
        # Python callback'lerinin garbage collector tarafından silinmesini önlemek için referans tablosu:
        self._c_callbacks: List[Any] = []

    def register(self, message_class: int, service: int, command: int,
                 callback: Callable[[DeosMessage], None],
                 target_node: int = NODE_GLOBAL_BROADCAST) -> int:
        """
        C motoruna yeni bir callback kaydeder.
        """
        def _c_shim(msg_ptr, user_data):
            msg = DeosMessage()
            msg._c = msg_ptr.contents
            try:
                callback(msg)
                return 0
            except Exception as e:
                print(f"[DeosRegistry Error in Python Callback]: {e}", file=sys.stderr)
                return -1

        c_func = _C_HANDLER_TYPE(_c_shim)
        self._c_callbacks.append(c_func)

        return _lib.deos_registry_register_for_node(
            self._reg_buf, target_node, message_class, service, command, c_func, None
        )

    def dispatch(self, message: DeosMessage) -> int:
        """
        C `deos_registry_dispatch` fonksiyonunu çağırarak mesajı ilgili handler'a yönlendirir.
        """
        return _lib.deos_registry_dispatch(self._reg_buf, ctypes.byref(message._c))


# ==============================================================================
# 5. Little-Endian Codec Yardımcıları (C Kütüphanesini Çağırır)
# ==============================================================================

def put_u16_le(val: int) -> bytes:
    buf = ctypes.create_string_buffer(2)
    _lib.deos_put_u16_le(buf, val)
    return buf.raw[:2]

def get_u16_le(data: bytes) -> int:
    return _lib.deos_get_u16_le(data[:2])

def put_i16_le(val: int) -> bytes:
    buf = ctypes.create_string_buffer(2)
    _lib.deos_put_i16_le(buf, val)
    return buf.raw[:2]

def get_i16_le(data: bytes) -> int:
    return _lib.deos_get_i16_le(data[:2])

def put_u32_le(val: int) -> bytes:
    buf = ctypes.create_string_buffer(4)
    _lib.deos_put_u32_le(buf, val)
    return buf.raw[:4]

def get_u32_le(data: bytes) -> int:
    return _lib.deos_get_u32_le(data[:4])

def put_i32_le(val: int) -> bytes:
    buf = ctypes.create_string_buffer(4)
    _lib.deos_put_i32_le(buf, val)
    return buf.raw[:4]

def get_i32_le(data: bytes) -> int:
    return _lib.deos_get_i32_le(data[:4])

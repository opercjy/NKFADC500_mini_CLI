import errno
import socket
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

try:
    import fcntl
except ImportError:
    fcntl = None

try:
    from caen_libs import caenhvwrapper as hv

    CAEN_WRAPPER_AVAILABLE = True
except ImportError:
    hv = None
    CAEN_WRAPPER_AVAILABLE = False


# =============================================================================
# CAEN Standard Channel Status Bitmask Definitions (CAENHVWrapper Spec)
# =============================================================================
class CaenStatusBits:
    ON = 1 << 0          # Bit 0: Channel is ON
    RAMP_UP = 1 << 1     # Bit 1: Channel is ramping up
    RAMP_DOWN = 1 << 2   # Bit 2: Channel is ramping down
    OVC = 1 << 3         # Bit 3: Over-Current condition
    OVV = 1 << 4         # Bit 4: Over-Voltage condition
    UNV = 1 << 5         # Bit 5: Under-Voltage condition
    EXT_TRIP = 1 << 6    # Bit 6: External Trip
    MAXV = 1 << 7        # Bit 7: VMon reached SVMax hardware clamp
    EXT_DISABLE = 1 << 8 # Bit 8: External Disable / Interlock open
    INT_TRIP = 1 << 9    # Bit 9: Internal Over-Current Trip latched
    CALIB_ERR = 1 << 10  # Bit 10: Calibration error
    UNPLUGGED = 1 << 11  # Bit 11: Module/Cable unplugged


@dataclass
class HvSystemProfile:
    model_id: str
    display_name: str
    caen_sys_enum_name: str
    topology: str                  # "UNIVERSAL_CRATE" or "NIM_DESKTOP_8CH"
    default_channels: int
    default_slot: int
    supports_tcp: bool
    supports_usb: bool
    exclusive_lock_warning: bool   # True for 8-Ch NIM / Desktop USB & single-socket TCP
    max_voltage_v: float
    max_current_ua: float
    description_ko: str


HV_SYSTEM_PROFILES: Dict[str, HvSystemProfile] = {
    "SY5527": HvSystemProfile(
        model_id="SY5527",
        display_name="SY5527 (Universal Crate Server)",
        caen_sys_enum_name="SY5527",
        topology="UNIVERSAL_CRATE",
        default_channels=8,
        default_slot=0,
        supports_tcp=True,
        supports_usb=False,
        exclusive_lock_warning=False,
        max_voltage_v=4000.0,
        max_current_ua=3000.0,
        description_ko=(
            "CAEN SY5527 Universal Multichannel Mainframe (TCP/IP 서버 방식, Slot/Channel 계층 주소 지정). "
            "내장 CPU 서버를 통해 원격 다중 채널 제어·모니터링 및 DAQ 탭 실시간 연동 지원."
        ),
    ),
    "SY4527LC": HvSystemProfile(
        model_id="SY4527LC",
        display_name="SY4527LC (Universal Low-Cost Crate)",
        caen_sys_enum_name="SY4527",
        topology="UNIVERSAL_CRATE",
        default_channels=8,
        default_slot=0,
        supports_tcp=True,
        supports_usb=False,
        exclusive_lock_warning=False,
        max_voltage_v=4000.0,
        max_current_ua=3000.0,
        description_ko=(
            "CAEN SY4527LC / SY4527 Universal Mainframe (TCP/IP 서버 방식, SY4527 프로토콜 호환, Slot/Channel 계층 제어). "
            "8채널/12채널/24채널 HV 보드(A1535, A7030 등) 장착 슬롯 지정 가능."
        ),
    ),
    "SY4527": HvSystemProfile(
        model_id="SY4527",
        display_name="SY4527 (Universal Mainframe)",
        caen_sys_enum_name="SY4527",
        topology="UNIVERSAL_CRATE",
        default_channels=8,
        default_slot=0,
        supports_tcp=True,
        supports_usb=False,
        exclusive_lock_warning=False,
        max_voltage_v=4000.0,
        max_current_ua=3000.0,
        description_ko="CAEN SY4527 Universal Multichannel Power Supply System (TCP/IP, Slot/Channel 계층 주소).",
    ),
    "N1470_NIM": HvSystemProfile(
        model_id="N1470_NIM",
        display_name="N1470 / N1471 (4/8-Ch NIM Module)",
        caen_sys_enum_name="N1470",
        topology="NIM_DESKTOP_8CH",
        default_channels=8,
        default_slot=0,
        supports_tcp=True,
        supports_usb=True,
        exclusive_lock_warning=True,
        max_voltage_v=8000.0,
        max_current_ua=3000.0,
        description_ko=(
            "CAEN N1470/N1471 NIM 고전압 모듈 (USB VCP / TCP/IP). "
            "⚠️ [단독 점유 주의] USB 가상 시리얼(/dev/ttyACM*, /dev/ttyUSB*) 및 단일 소켓 네트워크 인터페이스는 "
            "단일 프로세스 단독 점유(Exclusive Lock) 방식으로 동작하므로 GECO2020 등과 동시 접속 시 거부될 수 있습니다."
        ),
    ),
    "DT1470ET_DESKTOP": HvSystemProfile(
        model_id="DT1470ET_DESKTOP",
        display_name="DT1470ET / DT5533E (4/8-Ch Desktop)",
        caen_sys_enum_name="DT55XX",
        topology="NIM_DESKTOP_8CH",
        default_channels=8,
        default_slot=0,
        supports_tcp=True,
        supports_usb=True,
        exclusive_lock_warning=True,
        max_voltage_v=8000.0,
        max_current_ua=3000.0,
        description_ko=(
            "CAEN DT1470ET / DT8033 / DT5533E 탁상형(Desktop) 고전압 공급기 (USB / Ethernet). "
            "⚠️ [단독 점유 주의] USB 포트 및 단일 TCP 세션은 단독 점유(Single-Host Exclusive Lock)되므로 "
            "외부 제어 프로그램 종료 후 연결하거나 수동 폴백(Fallback) 모드를 활용하십시오."
        ),
    ),
    "SMARTHV": HvSystemProfile(
        model_id="SMARTHV",
        display_name="SMARTHV / V65XX (Modular HV)",
        caen_sys_enum_name="SMARTHV",
        topology="NIM_DESKTOP_8CH",
        default_channels=8,
        default_slot=0,
        supports_tcp=True,
        supports_usb=True,
        exclusive_lock_warning=True,
        max_voltage_v=5000.0,
        max_current_ua=2000.0,
        description_ko="CAEN SmartHV / V65XX 고전압 모듈 (USB / TCP/IP 단일 세션 점유 주의).",
    ),
}


@dataclass
class HvChannelState:
    index: int                     # Logical row 0..7 in 8-channel controller
    slot: int = 0                  # Board slot (0 for NIM/Desktop; 0..15 for SY5527/SY4527LC)
    hw_ch: int = 0                 # Hardware channel index on the board (0..7 or 0..23)
    fadc_map: int = -1             # Mapped NKFADC500 channel (0..3, or -1 if unmapped)
    label: str = ""
    vset: float = 1700.0           # Target voltage V0Set [V]
    iset: float = 100.0            # Current trip threshold I0Set [uA]
    vmon: float = 0.0              # Monitored voltage VMon [V]
    imon: float = 0.0              # Monitored current IMon [uA]
    ramp_up: float = 50.0          # Ramp-up rate RUp [V/s]
    ramp_down: float = 100.0       # Ramp-down rate RDWn [V/s]
    svmax: float = 2500.0          # Hardware max voltage limit SVMax [V]
    trip_time: float = 1.0         # Over-current trip time [s]
    pw: bool = False               # Power command state (True=ON, False=OFF)
    status_bits: int = 0           # Bitmask from CaenStatusBits
    overcurrent_since: float = 0.0 # Internal timestamp for trip integration in simulator

    def __post_init__(self):
        if not self.label:
            self.label = f"PMT_{self.index}"

    @property
    def is_tripped(self) -> bool:
        return bool(self.status_bits & (CaenStatusBits.INT_TRIP | CaenStatusBits.EXT_TRIP | CaenStatusBits.OVC | CaenStatusBits.OVV))

    @property
    def is_ramping(self) -> bool:
        return bool(self.status_bits & (CaenStatusBits.RAMP_UP | CaenStatusBits.RAMP_DOWN))

    @property
    def is_on(self) -> bool:
        return bool(self.status_bits & CaenStatusBits.ON) or self.pw

    @property
    def status_text(self) -> str:
        if self.status_bits & (CaenStatusBits.INT_TRIP | CaenStatusBits.EXT_TRIP):
            return "TRIP"
        if self.status_bits & CaenStatusBits.OVC:
            return "OVC"
        if self.status_bits & CaenStatusBits.OVV:
            return "OVV"
        if self.status_bits & CaenStatusBits.RAMP_UP:
            return "RAMP UP"
        if self.status_bits & CaenStatusBits.RAMP_DOWN:
            return "RAMP DN"
        if self.status_bits & CaenStatusBits.ON:
            return "ON"
        return "OFF"


# =============================================================================
# Hardware Abstraction Layer (HAL) Base Interface
# =============================================================================
class AbstractHvBackend(ABC):
    """
    Hardware Abstraction Layer (HAL) for CAEN High-Voltage Power Supplies.
    Decouples the GUI (HvTab & DaqTab) from specific transport/driver implementations:
      1. CaenHvWrapperBackend : Official C shared library (libcaenhvwrapper / caen_libs)
      2. CaenAsciiProtocolBackend : Direct ASCII protocol ($BD:..,CMD:..) over USB VCP or TCP socket
      3. VirtualTwinHvBackend : Physics-accurate ramp/trip digital twin simulator
    """

    def __init__(self, profile: HvSystemProfile):
        self.profile = profile
        self.connected = False
        self.endpoint_desc = ""

    @property
    @abstractmethod
    def backend_name(self) -> str:
        pass

    @abstractmethod
    def connect_device(
        self,
        link_type: str,
        address: str,
        username: str,
        password: str,
        channels: List[HvChannelState],
    ) -> Tuple[bool, str]:
        pass

    @abstractmethod
    def disconnect_device(self, channels: List[HvChannelState]) -> None:
        pass

    @abstractmethod
    def apply_channel_settings(self, ch_state: HvChannelState) -> Tuple[bool, str]:
        pass

    @abstractmethod
    def set_channel_power(self, ch_state: HvChannelState, power_on: bool) -> Tuple[bool, str]:
        pass

    @abstractmethod
    def poll_channels(self, channels: List[HvChannelState]) -> None:
        pass

    @abstractmethod
    def emergency_kill_all(self, channels: List[HvChannelState]) -> None:
        pass


# =============================================================================
# 1. Official CAENHVWrapper Backend (SY5527, SY4527LC, SY4527, N1470, DT55XX)
# =============================================================================
class CaenHvWrapperBackend(AbstractHvBackend):
    def __init__(self, profile: HvSystemProfile):
        super().__init__(profile)
        self._device = None

    @property
    def backend_name(self) -> str:
        return "CAENHVWrapper (Official Driver)"

    def _resolve_enums(self, link_type_str: str):
        sys_enum = getattr(hv.SystemType, self.profile.caen_sys_enum_name, hv.SystemType.SY4527)
        link_enum = hv.LinkType.TCPIP if link_type_str == "TCP/IP" else hv.LinkType.USB
        return sys_enum, link_enum

    def connect_device(
        self,
        link_type: str,
        address: str,
        username: str,
        password: str,
        channels: List[HvChannelState],
    ) -> Tuple[bool, str]:
        if not CAEN_WRAPPER_AVAILABLE:
            return False, "Python 'caen_libs' (caenhvwrapper) 패키지가 설치되어 있지 않습니다."

        sys_enum, link_enum = self._resolve_enums(link_type)
        try:
            self._device = hv.Device.open(sys_enum, link_enum, address, username, password)
            self.connected = True
            self.endpoint_desc = f"{self.profile.model_id} @ {address} ({link_type})"
            self.poll_channels(channels)
            return True, f"CAENHVWrapper 세션 연결 성공: {self.endpoint_desc}"
        except Exception as exc:
            self._device = None
            self.connected = False
            err_msg = str(exc)
            if self.profile.exclusive_lock_warning:
                err_msg += (
                    " [단독 점유 진단: 8채널 NIM/탁상형 장비가 GECO2020, 웹 UI, 또는 다른 터미널에 의해 "
                    "단독 점유(Exclusive Lock)되었거나 응답하지 않습니다.]"
                )
            return False, err_msg

    def disconnect_device(self, channels: List[HvChannelState]) -> None:
        if self._device is not None:
            try:
                self._device.close()
            except Exception:
                pass
            self._device = None
        self.connected = False

    def apply_channel_settings(self, ch_state: HvChannelState) -> Tuple[bool, str]:
        if not self.connected or self._device is None:
            return False, "Not connected to CAEN hardware."
        slot = int(ch_state.slot)
        ch_list = [int(ch_state.hw_ch)]
        try:
            v_clamped = min(float(ch_state.vset), float(ch_state.svmax), self.profile.max_voltage_v)
            ch_state.vset = v_clamped
            for par_name, val in (
                ("SVMax", float(ch_state.svmax)),
                ("V0Set", float(v_clamped)),
                ("I0Set", float(ch_state.iset)),
                ("RUp", float(ch_state.ramp_up)),
                ("RDWn", float(ch_state.ramp_down)),
            ):
                try:
                    self._device.set_ch_param(slot, ch_list, par_name, val)
                except Exception:
                    pass
            return True, f"Slot {slot} Ch {ch_state.hw_ch}: V0Set={v_clamped:.1f}V, I0Set={ch_state.iset:.1f}uA 인가 완료"
        except Exception as exc:
            return False, f"Slot {slot} Ch {ch_state.hw_ch} 설정 실패: {exc}"

    def set_channel_power(self, ch_state: HvChannelState, power_on: bool) -> Tuple[bool, str]:
        if not self.connected or self._device is None:
            return False, "Not connected to CAEN hardware."
        slot = int(ch_state.slot)
        ch_list = [int(ch_state.hw_ch)]
        try:
            self._device.set_ch_param(slot, ch_list, "Pw", 1 if power_on else 0)
            ch_state.pw = power_on
            return True, f"Slot {slot} Ch {ch_state.hw_ch} Power -> {'ON' if power_on else 'OFF'}"
        except Exception as exc:
            return False, f"Slot {slot} Ch {ch_state.hw_ch} Power 전환 실패: {exc}"

    def poll_channels(self, channels: List[HvChannelState]) -> None:
        if not self.connected or self._device is None:
            return
        # Group channels by slot for batch crate queries (essential for SY5527 / SY4527LC efficiency)
        by_slot: Dict[int, List[HvChannelState]] = {}
        for ch in channels:
            by_slot.setdefault(int(ch.slot), []).append(ch)

        for slot, slot_chs in by_slot.items():
            hw_indices = [int(c.hw_ch) for c in slot_chs]
            try:
                vmon_vals = self._device.get_ch_param(slot, hw_indices, "VMon")
                imon_vals = self._device.get_ch_param(slot, hw_indices, "IMon")
                stat_vals = self._device.get_ch_param(slot, hw_indices, "Status")
                for idx, ch_obj in enumerate(slot_chs):
                    if idx < len(vmon_vals) and vmon_vals[idx] is not None:
                        ch_obj.vmon = float(vmon_vals[idx])
                    if idx < len(imon_vals) and imon_vals[idx] is not None:
                        ch_obj.imon = float(imon_vals[idx])
                    if idx < len(stat_vals) and stat_vals[idx] is not None:
                        ch_obj.status_bits = int(stat_vals[idx])
                        ch_obj.pw = bool(ch_obj.status_bits & (CaenStatusBits.ON | CaenStatusBits.RAMP_UP))
            except Exception:
                pass

    def emergency_kill_all(self, channels: List[HvChannelState]) -> None:
        if not self.connected or self._device is None:
            return
        for ch in channels:
            try:
                self._device.set_ch_param(int(ch.slot), [int(ch.hw_ch)], "Pw", 0)
            except Exception:
                pass
            ch.pw = False


# =============================================================================
# 2. Direct CAEN ASCII Protocol Backend ($BD:xx,CMD:.. over USB VCP or TCP)
#    Designed for 8-Ch NIM (N1470/N1471) & Desktop (DT1470ET/DT5533E)
#    with Explicit Single-Process Exclusive Lock Detection
# =============================================================================
class CaenAsciiProtocolBackend(AbstractHvBackend):
    def __init__(self, profile: HvSystemProfile):
        super().__init__(profile)
        self._sock: Optional[socket.socket] = None
        self._serial_fp = None
        self._board_addr = 0

    @property
    def backend_name(self) -> str:
        return "CAEN ASCII Protocol (Direct USB-VCP / TCP)"

    def connect_device(
        self,
        link_type: str,
        address: str,
        username: str,
        password: str,
        channels: List[HvChannelState],
    ) -> Tuple[bool, str]:
        self.disconnect_device(channels)
        try:
            if link_type == "USB":
                dev_path = address.strip()
                if dev_path.isdigit():
                    dev_path = f"/dev/ttyACM{dev_path}"
                # Open USB Virtual COM device and acquire non-blocking exclusive flock
                self._serial_fp = open(dev_path, "r+b", buffering=0)
                if fcntl is not None:
                    try:
                        fcntl.flock(self._serial_fp.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                    except OSError as exc:
                        self._serial_fp.close()
                        self._serial_fp = None
                        if exc.errno in (errno.EACCES, errno.EAGAIN, errno.EBUSY):
                            return (
                                False,
                                f"USB 포트 '{dev_path}'가 다른 프로세스(GECO2020 등)에 의해 단독 점유(Exclusive Lock)되어 있습니다.",
                            )
                        raise
                self.connected = True
                self.endpoint_desc = f"{self.profile.model_id} @ {dev_path} (USB Exclusive)"
                return True, f"USB VCP 단독 점유 잠금 획득 및 연결 성공: {self.endpoint_desc}"
            else:
                host = address.strip()
                port = 1470
                if ":" in host:
                    host_part, port_part = host.rsplit(":", 1)
                    host = host_part.strip()
                    port = int(port_part)
                sock = socket.create_connection((host, port), timeout=1.5)
                sock.settimeout(1.0)
                self._sock = sock
                self.connected = True
                self.endpoint_desc = f"{self.profile.model_id} @ {host}:{port} (TCP ASCII)"
                return True, f"TCP ASCII 세션 연결 성공: {self.endpoint_desc}"
        except Exception as exc:
            self.disconnect_device(channels)
            return (
                False,
                f"직접 연결 실패 ({exc}) — 장비 전원이 꺼져 있거나 다른 호스트가 세션을 단독 점유 중일 수 있습니다.",
            )

    def _send_ascii_cmd(self, cmd_str: str) -> str:
        payload = (cmd_str.strip() + "\r\n").encode("ascii")
        if self._sock is not None:
            self._sock.sendall(payload)
            return self._sock.recv(512).decode("ascii", errors="ignore").strip()
        if self._serial_fp is not None:
            self._serial_fp.write(payload)
            return self._serial_fp.readline(512).decode("ascii", errors="ignore").strip()
        return ""

    def disconnect_device(self, channels: List[HvChannelState]) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            except Exception:
                pass
            self._sock = None
        if self._serial_fp is not None:
            try:
                if fcntl is not None:
                    fcntl.flock(self._serial_fp.fileno(), fcntl.LOCK_UN)
                self._serial_fp.close()
            except Exception:
                pass
            self._serial_fp = None
        self.connected = False

    def apply_channel_settings(self, ch_state: HvChannelState) -> Tuple[bool, str]:
        if not self.connected:
            return False, "Not connected."
        bd = int(ch_state.slot)
        ch = int(ch_state.hw_ch)
        v_clamped = min(float(ch_state.vset), float(ch_state.svmax), self.profile.max_voltage_v)
        ch_state.vset = v_clamped
        try:
            self._send_ascii_cmd(f"$BD:{bd:02d},CMD:SET,CH:{ch},PAR:VSET,VAL:{v_clamped:.1f}")
            self._send_ascii_cmd(f"$BD:{bd:02d},CMD:SET,CH:{ch},PAR:ISET,VAL:{ch_state.iset:.2f}")
            self._send_ascii_cmd(f"$BD:{bd:02d},CMD:SET,CH:{ch},PAR:RUP,VAL:{ch_state.ramp_up:.1f}")
            self._send_ascii_cmd(f"$BD:{bd:02d},CMD:SET,CH:{ch},PAR:RDW,VAL:{ch_state.ramp_down:.1f}")
            return True, f"BD {bd:02d} Ch {ch}: VSET={v_clamped:.1f}V, ISET={ch_state.iset:.1f}uA 전송 완료"
        except Exception as exc:
            return False, f"ASCII 명령 실패: {exc}"

    def set_channel_power(self, ch_state: HvChannelState, power_on: bool) -> Tuple[bool, str]:
        if not self.connected:
            return False, "Not connected."
        bd = int(ch_state.slot)
        ch = int(ch_state.hw_ch)
        par = "ON" if power_on else "OFF"
        try:
            self._send_ascii_cmd(f"$BD:{bd:02d},CMD:SET,CH:{ch},PAR:{par}")
            ch_state.pw = power_on
            return True, f"BD {bd:02d} Ch {ch} -> {par}"
        except Exception as exc:
            return False, f"Power 제어 실패: {exc}"

    def _parse_val(self, resp: str) -> Optional[float]:
        if "VAL:" not in resp:
            return None
        tail = resp.split("VAL:", 1)[1].split(";")[0].strip()
        try:
            return float(tail)
        except ValueError:
            return None

    def poll_channels(self, channels: List[HvChannelState]) -> None:
        if not self.connected:
            return
        for ch_obj in channels:
            bd = int(ch_obj.slot)
            ch = int(ch_obj.hw_ch)
            try:
                rv = self._parse_val(self._send_ascii_cmd(f"$BD:{bd:02d},CMD:MON,CH:{ch},PAR:VMON"))
                ri = self._parse_val(self._send_ascii_cmd(f"$BD:{bd:02d},CMD:MON,CH:{ch},PAR:IMON"))
                rs = self._parse_val(self._send_ascii_cmd(f"$BD:{bd:02d},CMD:MON,CH:{ch},PAR:STAT"))
                if rv is not None:
                    ch_obj.vmon = rv
                if ri is not None:
                    ch_obj.imon = ri
                if rs is not None:
                    ch_obj.status_bits = int(rs)
                    ch_obj.pw = bool(ch_obj.status_bits & (CaenStatusBits.ON | CaenStatusBits.RAMP_UP))
            except Exception:
                pass

    def emergency_kill_all(self, channels: List[HvChannelState]) -> None:
        if not self.connected:
            return
        for ch_obj in channels:
            try:
                self._send_ascii_cmd(f"$BD:{int(ch_obj.slot):02d},CMD:SET,CH:{int(ch_obj.hw_ch)},PAR:OFF")
            except Exception:
                pass
            ch_obj.pw = False


# =============================================================================
# 3. Physics-Accurate Digital Twin Simulator Backend (Ramp dV/dt, SVMax, OVC Trip)
# =============================================================================
class VirtualTwinHvBackend(AbstractHvBackend):
    def __init__(self, profile: HvSystemProfile):
        super().__init__(profile)
        self._last_poll_ts = time.monotonic()

    @property
    def backend_name(self) -> str:
        return "CAEN Digital Twin Simulator (Ramp/Trip Physics)"

    def connect_device(
        self,
        link_type: str,
        address: str,
        username: str,
        password: str,
        channels: List[HvChannelState],
    ) -> Tuple[bool, str]:
        self.connected = True
        self._last_poll_ts = time.monotonic()
        self.endpoint_desc = f"{self.profile.model_id} @ {address} ({link_type} Digital Twin)"
        return True, f"디지털 트윈 시뮬레이터 세션 활성화: {self.endpoint_desc}"

    def disconnect_device(self, channels: List[HvChannelState]) -> None:
        self.connected = False

    def apply_channel_settings(self, ch_state: HvChannelState) -> Tuple[bool, str]:
        if not self.connected:
            return False, "Simulator offline."
        v_clamped = min(float(ch_state.vset), float(ch_state.svmax), self.profile.max_voltage_v)
        max_capped = v_clamped < ch_state.vset
        ch_state.vset = v_clamped
        if max_capped:
            ch_state.status_bits |= CaenStatusBits.MAXV
        else:
            ch_state.status_bits &= ~CaenStatusBits.MAXV
        return (
            True,
            f"[Sim] Slot {ch_state.slot} Ch {ch_state.hw_ch}: V0Set={v_clamped:.1f}V, I0Set={ch_state.iset:.1f}uA, "
            f"RUp={ch_state.ramp_up:.0f}V/s, RDWn={ch_state.ramp_down:.0f}V/s",
        )

    def set_channel_power(self, ch_state: HvChannelState, power_on: bool) -> Tuple[bool, str]:
        if not self.connected:
            return False, "Simulator offline."
        if power_on:
            # Clear prior trip latch when user explicitly re-arms power
            ch_state.status_bits &= ~(CaenStatusBits.INT_TRIP | CaenStatusBits.OVC | CaenStatusBits.OVV)
            ch_state.overcurrent_since = 0.0
            ch_state.pw = True
            return True, f"[Sim] Slot {ch_state.slot} Ch {ch_state.hw_ch} Power ON (Ramping at {ch_state.ramp_up:.0f} V/s)"
        else:
            ch_state.pw = False
            return True, f"[Sim] Slot {ch_state.slot} Ch {ch_state.hw_ch} Power OFF (Ramping down at {ch_state.ramp_down:.0f} V/s)"

    def poll_channels(self, channels: List[HvChannelState]) -> None:
        if not self.connected:
            return
        now = time.monotonic()
        dt = max(0.05, min(2.5, now - self._last_poll_ts))
        self._last_poll_ts = now

        for ch in channels:
            target_v = min(ch.vset, ch.svmax) if ch.pw else 0.0
            dv = target_v - ch.vmon
            bits = ch.status_bits & (CaenStatusBits.INT_TRIP | CaenStatusBits.MAXV)

            if abs(dv) > 0.25:
                if dv > 0.0:
                    step = max(5.0, ch.ramp_up) * dt
                    ch.vmon = min(target_v, ch.vmon + step)
                    bits |= CaenStatusBits.ON | CaenStatusBits.RAMP_UP
                else:
                    step = max(5.0, ch.ramp_down) * dt
                    ch.vmon = max(target_v, ch.vmon - step)
                    bits |= CaenStatusBits.RAMP_DOWN
                    if ch.pw or ch.vmon > 1.0:
                        bits |= CaenStatusBits.ON
            else:
                ch.vmon = target_v
                if ch.pw and ch.vmon > 0.5:
                    bits |= CaenStatusBits.ON

            # Realistic PMT voltage-divider / dark-current load model (~1.2 uA at 1700 V + small jitter)
            if ch.vmon > 1.0:
                jitter = ((int(now * 10) + ch.index * 7) % 5 - 2) * 0.02
                ch.imon = max(0.01, (ch.vmon / 1700.0) * 1.20 + jitter)
            else:
                ch.vmon = 0.0
                ch.imon = 0.0

            # Over-Current (OVC) & Trip integration check
            if ch.imon > ch.iset and ch.iset > 0.0:
                bits |= CaenStatusBits.OVC
                if ch.overcurrent_since <= 0.0:
                    ch.overcurrent_since = now
                elif (now - ch.overcurrent_since) >= ch.trip_time:
                    bits |= CaenStatusBits.INT_TRIP
                    ch.pw = False
            else:
                ch.overcurrent_since = 0.0

            ch.status_bits = bits

    def emergency_kill_all(self, channels: List[HvChannelState]) -> None:
        for ch in channels:
            ch.pw = False
            ch.vmon = 0.0
            ch.imon = 0.0
            ch.status_bits = 0

from typing import Dict, List, Optional, Tuple

from PySide6.QtCore import QTimer, Qt, Signal
from PySide6.QtGui import QColor, QFont
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QGroupBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from core.HvBackend import (
    CAEN_WRAPPER_AVAILABLE,
    HV_SYSTEM_PROFILES,
    AbstractHvBackend,
    CaenAsciiProtocolBackend,
    CaenHvWrapperBackend,
    HvChannelState,
    HvSystemProfile,
    VirtualTwinHvBackend,
)

# Okabe-Ito Colorblind-Safe Palette for FADC Channel Mapping
OKABE_ITO_FADC_HEX = {
    0: "#0072B2",  # Ch 0: Blue
    1: "#D55E00",  # Ch 1: Vermilion
    2: "#009E73",  # Ch 2: Bluish Green
    3: "#CC79A7",  # Ch 3: Reddish Purple
    -1: "#607D8B", # Unmapped: Slate Gray
}


class HvTab(QWidget):
    sig_log = Signal(str, bool)
    sig_hv_updated = Signal(dict)  # Broadcasts live 8-channel & FADC-mapped telemetry to DaqTab

    COL_HV_CH = 0
    COL_FADC_MAP = 1
    COL_SLOT_CH = 2
    COL_VSET = 3
    COL_ISET = 4
    COL_RAMP = 5
    COL_SVMAX = 6
    COL_VMON = 7
    COL_IMON = 8
    COL_STATE = 9
    COL_POWER = 10

    def __init__(self):
        super().__init__()
        self.connected = False
        self.num_channels = 8
        self._updating_table = False

        # Initialize 8 HV channels (Ch 0..3 mapped by default to NKFADC500 Ch 0..3; Ch 4..7 unmapped/aux)
        self.channels: List[HvChannelState] = [
            HvChannelState(
                index=i,
                slot=0,
                hw_ch=i,
                fadc_map=i if i < 4 else -1,
                label=f"FADC_Ch{i}" if i < 4 else f"Aux_HV{i}",
                vset=1700.0,
                iset=100.0,
                ramp_up=50.0,
                ramp_down=100.0,
                svmax=2500.0,
            )
            for i in range(self.num_channels)
        ]

        self.backend: Optional[AbstractHvBackend] = None

        self.poll_timer = QTimer(self)
        self.poll_timer.timeout.connect(self.poll_hv_status)

        self.initUI()
        self._emit_hv_snapshot()

    def initUI(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        # 1. Top Mode Switch + Hardware Architecture Banner
        mode_layout = QHBoxLayout()
        self.cb_enable_scm = QCheckBox(
            "🔌 CAEN HV Remote Control 활성화 (Universal SY5527 / SY4527LC 서버 && 8-Ch NIM / 탁상형 TCP/IP · USB 제어)"
        )
        self.cb_enable_scm.setStyleSheet("font-weight: bold; color: #0072B2; font-size: 14px;")
        self.cb_enable_scm.setChecked(False)
        self.cb_enable_scm.toggled.connect(self.toggle_mode)
        mode_layout.addWidget(self.cb_enable_scm)
        mode_layout.addStretch()
        layout.addLayout(mode_layout)

        self.lbl_analog = QLabel(
            "⚠️ Analog / Manual HV Mode (비네트워크 수동 기록 모드)\n\n"
            "수동 다이얼 방식의 아날로그 고전압 공급기(ORTEC NIM 등)를 사용 중이거나 원격 제어를 비활성화한 상태입니다.\n"
            "아래 8채널 테이블의 VSet / ISet 열에 인가 전압·전류를 기입하면 DAQ 탭 및 Run Database에 자동 연동·기록됩니다."
        )
        self.lbl_analog.setAlignment(Qt.AlignCenter)
        self.lbl_analog.setStyleSheet(
            """
            font-weight: bold; color: #5D4037;
            background-color: #EFEBE9;
            border: 2px dashed #8D6E63;
            border-radius: 8px;
            padding: 16px;
            font-size: 13px;
            """
        )
        layout.addWidget(self.lbl_analog)

        self.caen_widget = QWidget()
        caen_layout = QVBoxLayout(self.caen_widget)
        caen_layout.setContentsMargins(0, 0, 0, 0)
        caen_layout.setSpacing(8)

        # 2. Connection & Hardware Abstraction Layer (HAL) Configuration Group
        self.conn_group = QGroupBox("CAEN High-Voltage Server && 8-Channel Module Connection (HAL)")
        self.conn_group.setStyleSheet(
            "QGroupBox { border: 2px solid #0072B2; border-radius: 6px; margin-top: 10px; background-color: #E3F2FD;} "
            "QGroupBox::title { color: #0072B2; font-weight:bold;}"
        )
        conn_vbox = QVBoxLayout(self.conn_group)
        conn_vbox.setContentsMargins(10, 16, 10, 10)
        conn_vbox.setSpacing(6)

        row1 = QHBoxLayout()
        self.combo_sys = QComboBox()
        self.combo_sys.setStyleSheet("font-weight:bold; color:#D55E00;")
        for model_key, prof in HV_SYSTEM_PROFILES.items():
            self.combo_sys.addItem(prof.display_name, model_key)
        self.combo_sys.currentIndexChanged.connect(self.on_model_changed)

        self.combo_driver = QComboBox()
        self.combo_driver.setStyleSheet("font-weight:bold; color:#009E73;")
        self.combo_driver.addItems(
            [
                "Auto (CAEN Driver -> Digital Twin)",
                "CAENHVWrapper (C Shared Library)",
                "ASCII Protocol (Direct USB-VCP / TCP)",
                "Digital Twin Simulator (Ramp/Trip Physics)",
            ]
        )

        self.combo_link = QComboBox()
        self.combo_link.addItems(["TCP/IP", "USB"])
        self.combo_link.setStyleSheet("font-weight:bold; color:#0072B2;")
        self.combo_link.currentIndexChanged.connect(self.on_link_type_changed)

        self.spin_default_slot = QSpinBox()
        self.spin_default_slot.setRange(0, 15)
        self.spin_default_slot.setValue(0)
        self.spin_default_slot.setToolTip(
            "Universal Mainframe (SY5527 / SY4527LC) 장착 HV 보드 슬롯 번호 (변경 시 8채널 Slot 일괄 갱신)"
        )
        self.spin_default_slot.valueChanged.connect(self.on_default_slot_changed)

        row1.addWidget(QLabel("Model:"))
        row1.addWidget(self.combo_sys, stretch=2)
        row1.addWidget(QLabel("HAL Backend:"))
        row1.addWidget(self.combo_driver, stretch=2)
        row1.addWidget(QLabel("Link:"))
        row1.addWidget(self.combo_link)
        row1.addWidget(QLabel("Crate Slot:"))
        row1.addWidget(self.spin_default_slot)
        conn_vbox.addLayout(row1)

        row2 = QHBoxLayout()
        self.lbl_ip_node = QLabel("Server IP / Host:")
        self.input_ip = QLineEdit("192.168.0.10")
        self.input_user = QLineEdit("admin")
        self.input_pwd = QLineEdit("admin")
        self.input_pwd.setEchoMode(QLineEdit.Password)
        self.input_user.setMaximumWidth(90)
        self.input_pwd.setMaximumWidth(90)

        self.btn_connect = QPushButton("🔗 Connect CAEN HV")
        self.btn_connect.setStyleSheet(
            "background-color: #009E73; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;"
        )
        self.btn_connect.clicked.connect(self.toggle_caen)

        row2.addWidget(self.lbl_ip_node)
        row2.addWidget(self.input_ip, stretch=2)
        row2.addWidget(QLabel("User:"))
        row2.addWidget(self.input_user)
        row2.addWidget(QLabel("Pass:"))
        row2.addWidget(self.input_pwd)
        row2.addWidget(self.btn_connect)
        conn_vbox.addLayout(row2)

        self.lbl_topology_info = QLabel("")
        self.lbl_topology_info.setWordWrap(True)
        self.lbl_topology_info.setStyleSheet(
            "color: #263238; background-color: #FFFFFF; border: 1px solid #90CAF9; border-radius: 4px; padding: 6px; font-size: 12px;"
        )
        conn_vbox.addWidget(self.lbl_topology_info)
        caen_layout.addWidget(self.conn_group)

        # 3. Exclusive Lock / Offline Fallback Notice Group
        self.caen_fallback_widget = QGroupBox(
            "🔒 Exclusive Session Lock && Manual Database Fallback (단독 점유 / 오프라인 수동 보존)"
        )
        self.caen_fallback_widget.setFont(QFont("Arial", 10, QFont.Bold))
        self.caen_fallback_widget.setStyleSheet(
            """
            QGroupBox {
                border: 2px solid #E69F00;
                border-radius: 6px;
                background-color: #FFF8E1;
                margin-top: 6px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 10px;
                color: #D55E00;
            }
            """
        )
        fb_layout = QHBoxLayout(self.caen_fallback_widget)
        fb_layout.setContentsMargins(10, 16, 10, 8)

        self.lbl_fb_notice = QLabel(
            "※ 8채널 NIM(N1470)·탁상형(DT1470ET) USB/단일소켓 단독 점유(Exclusive Lock)로 연결이 거부되거나 오프라인일 때, "
            "아래 표의 VSet/ISet 또는 빠른 수동 입력값이 DAQ 탭과 DB에 기록됩니다."
        )
        self.lbl_fb_notice.setWordWrap(True)
        self.lbl_fb_notice.setStyleSheet("color: #B71C1C; font-weight: bold; font-size: 12px;")
        fb_layout.addWidget(self.lbl_fb_notice, stretch=2)

        self.input_caen_fallback_v = QLineEdit()
        self.input_caen_fallback_v.setPlaceholderText("Quick Fallback V (e.g. 1700)")
        self.input_caen_fallback_v.setMaximumWidth(170)
        self.input_caen_fallback_v.setStyleSheet("background-color: white; font-weight: bold;")
        self.input_caen_fallback_v.textChanged.connect(lambda _: self._emit_hv_snapshot())
        fb_layout.addWidget(self.input_caen_fallback_v)

        self.input_caen_fallback_i = QLineEdit()
        self.input_caen_fallback_i.setPlaceholderText("Quick Fallback I (uA)")
        self.input_caen_fallback_i.setMaximumWidth(150)
        self.input_caen_fallback_i.setStyleSheet("background-color: white; font-weight: bold;")
        self.input_caen_fallback_i.textChanged.connect(lambda _: self._emit_hv_snapshot())
        fb_layout.addWidget(self.input_caen_fallback_i)

        caen_layout.addWidget(self.caen_fallback_widget)

        # 4. 8-Channel HV Configuration, FADC Mapping & Live Telemetry Table
        ctrl_group = QGroupBox("8-Channel HV Controller && NKFADC500 Ch 0–3 Mapping Matrix")
        ctrl_layout = QVBoxLayout(ctrl_group)
        ctrl_layout.setContentsMargins(8, 16, 8, 8)

        self.table = QTableWidget(self.num_channels, 11)
        self.table.setHorizontalHeaderLabels(
            [
                "HV Ch",
                "FADC Map",
                "Slot:Ch",
                "VSet (V)",
                "ISet (uA)",
                "RUp/RDn (V/s)",
                "SVMax (V)",
                "VMon (V)",
                "IMon (uA)",
                "State",
                "Power Control",
            ]
        )
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.verticalHeader().setVisible(False)
        self.table.setMinimumHeight(290)
        self.table.setStyleSheet(
            "QTableWidget { background-color: #FFFFFF; color: #263238; gridline-color: #CFD8DC; font-size: 12px;}"
        )
        self.table.itemChanged.connect(self.on_table_item_changed)

        self.setup_table_mode(is_caen=False)
        ctrl_layout.addWidget(self.table)

        # Action Bar
        btn_bar = QHBoxLayout()
        self.btn_apply = QPushButton("⚡ Apply All Settings (VSet / ISet / Ramp / SVMax)")
        self.btn_apply.setStyleSheet(
            "background-color: #0072B2; color: white; font-weight: bold; padding: 8px 12px; border-radius: 4px;"
        )
        self.btn_apply.clicked.connect(self.apply_all_settings)

        self.btn_mapped_on = QPushButton("🟢 Mapped FADC Ch (0–3) ON")
        self.btn_mapped_on.setStyleSheet(
            "background-color: #009E73; color: white; font-weight: bold; padding: 8px 12px; border-radius: 4px;"
        )
        self.btn_mapped_on.clicked.connect(lambda: self.set_all_mapped_power(True))

        self.btn_all_off = QPushButton("🔴 All 8 Ch RAMP DOWN (OFF)")
        self.btn_all_off.setStyleSheet(
            "background-color: #E69F00; color: black; font-weight: bold; padding: 8px 12px; border-radius: 4px;"
        )
        self.btn_all_off.clicked.connect(lambda: self.set_all_channels_power(False))

        self.btn_kill = QPushButton("🛑 EMERGENCY KILL ALL")
        self.btn_kill.setStyleSheet(
            "background-color: #D55E00; color: white; font-weight: bold; padding: 8px 12px; border-radius: 4px;"
        )
        self.btn_kill.clicked.connect(self.force_shutdown)

        btn_bar.addWidget(self.btn_apply)
        btn_bar.addWidget(self.btn_mapped_on)
        btn_bar.addWidget(self.btn_all_off)
        btn_bar.addWidget(self.btn_kill)
        ctrl_layout.addLayout(btn_bar)

        caen_layout.addWidget(ctrl_group)

        self.conn_group.setVisible(False)
        self.caen_fallback_widget.setVisible(False)
        self.btn_apply.setVisible(False)
        self.btn_mapped_on.setVisible(False)
        self.btn_all_off.setVisible(False)
        self.btn_kill.setVisible(False)

        layout.addWidget(self.caen_widget)
        self.on_model_changed()

    def current_profile(self) -> HvSystemProfile:
        key = self.combo_sys.currentData() or "SY5527"
        return HV_SYSTEM_PROFILES.get(key, HV_SYSTEM_PROFILES["SY5527"])

    def on_model_changed(self):
        prof = self.current_profile()
        self.spin_default_slot.setEnabled(prof.topology == "UNIVERSAL_CRATE")
        if prof.topology != "UNIVERSAL_CRATE":
            self.spin_default_slot.setValue(0)
        if not prof.supports_usb and self.combo_link.currentText() == "USB":
            self.combo_link.setCurrentText("TCP/IP")
        self.lbl_topology_info.setText(
            f"ℹ️ [{prof.display_name}] Max: {prof.max_voltage_v:.0f} V / {prof.max_current_ua:.0f} uA — {prof.description_ko}"
        )
        self._emit_hv_snapshot()

    def on_default_slot_changed(self, new_slot: int):
        self._updating_table = True
        try:
            for row, ch in enumerate(self.channels):
                ch.slot = int(new_slot)
                item = self.table.item(row, self.COL_SLOT_CH)
                if item is not None:
                    item.setText(f"{ch.slot}:{ch.hw_ch}")
        finally:
            self._updating_table = False
        self._emit_hv_snapshot()

    def on_link_type_changed(self):
        if self.combo_link.currentText() == "USB":
            self.lbl_ip_node.setText("USB Node / VCP:")
            if self.input_ip.text().startswith("192."):
                self.input_ip.setText("/dev/ttyACM0")
            self.input_user.setEnabled(False)
            self.input_pwd.setEnabled(False)
        else:
            self.lbl_ip_node.setText("Server IP / Host:")
            if self.input_ip.text().startswith("/dev/") or self.input_ip.text() == "0":
                self.input_ip.setText("192.168.0.10")
            self.input_user.setEnabled(True)
            self.input_pwd.setEnabled(True)

    def setup_table_mode(self, is_caen: bool):
        self._updating_table = True
        try:
            for row, ch in enumerate(self.channels):
                # Col 0: HV Ch
                item_ch = QTableWidgetItem(f"HV {row}")
                item_ch.setFlags(Qt.ItemIsEnabled)
                item_ch.setTextAlignment(Qt.AlignCenter)
                item_ch.setFont(QFont("Consolas", 10, QFont.Bold))
                self.table.setItem(row, self.COL_HV_CH, item_ch)

                # Col 1: FADC Map ComboBox
                combo_map = self.table.cellWidget(row, self.COL_FADC_MAP)
                if combo_map is None:
                    combo_map = QComboBox()
                    combo_map.addItem("FADC Ch 0", 0)
                    combo_map.addItem("FADC Ch 1", 1)
                    combo_map.addItem("FADC Ch 2", 2)
                    combo_map.addItem("FADC Ch 3", 3)
                    combo_map.addItem("Unmapped", -1)
                    idx = combo_map.findData(ch.fadc_map)
                    combo_map.setCurrentIndex(idx if idx >= 0 else 4)
                    combo_map.currentIndexChanged.connect(lambda _=0, r=row: self.on_fadc_map_changed(r))
                    self.table.setCellWidget(row, self.COL_FADC_MAP, combo_map)
                self._style_fadc_map_combo(combo_map, ch.fadc_map)

                # Col 2: Slot:Ch
                item_sc = QTableWidgetItem(f"{ch.slot}:{ch.hw_ch}")
                item_sc.setTextAlignment(Qt.AlignCenter)
                self.table.setItem(row, self.COL_SLOT_CH, item_sc)

                # Col 3: VSet
                item_vset = QTableWidgetItem(f"{ch.vset:.1f}")
                item_vset.setTextAlignment(Qt.AlignCenter)
                self.table.setItem(row, self.COL_VSET, item_vset)

                # Col 4: ISet
                item_iset = QTableWidgetItem(f"{ch.iset:.1f}")
                item_iset.setTextAlignment(Qt.AlignCenter)
                self.table.setItem(row, self.COL_ISET, item_iset)

                # Col 5: RUp/RDn
                item_ramp = QTableWidgetItem(f"{ch.ramp_up:.0f}/{ch.ramp_down:.0f}")
                item_ramp.setTextAlignment(Qt.AlignCenter)
                self.table.setItem(row, self.COL_RAMP, item_ramp)

                # Col 6: SVMax
                item_svmax = QTableWidgetItem(f"{ch.svmax:.0f}")
                item_svmax.setTextAlignment(Qt.AlignCenter)
                self.table.setItem(row, self.COL_SVMAX, item_svmax)

                # Col 7: VMon
                item_vmon = QTableWidgetItem("N/A" if not is_caen else f"{ch.vmon:.2f}")
                item_vmon.setFlags(Qt.ItemIsEnabled)
                item_vmon.setTextAlignment(Qt.AlignCenter)
                item_vmon.setForeground(QColor("#9E9E9E" if not is_caen else "#D55E00"))
                item_vmon.setFont(QFont("Consolas", 10, QFont.Bold))
                self.table.setItem(row, self.COL_VMON, item_vmon)

                # Col 8: IMon
                item_imon = QTableWidgetItem("N/A" if not is_caen else f"{ch.imon:.2f}")
                item_imon.setFlags(Qt.ItemIsEnabled)
                item_imon.setTextAlignment(Qt.AlignCenter)
                item_imon.setForeground(QColor("#9E9E9E" if not is_caen else "#0072B2"))
                item_imon.setFont(QFont("Consolas", 10, QFont.Bold))
                self.table.setItem(row, self.COL_IMON, item_imon)

                # Col 9: State
                state_str = "MANUAL" if not is_caen else ch.status_text
                item_state = QTableWidgetItem(state_str)
                item_state.setFlags(Qt.ItemIsEnabled)
                item_state.setTextAlignment(Qt.AlignCenter)
                item_state.setFont(QFont("Consolas", 10, QFont.Bold))
                self.table.setItem(row, self.COL_STATE, item_state)

                # Col 10: Persistent Power Button / Manual Tag
                btn_pw = self.table.cellWidget(row, self.COL_POWER)
                if btn_pw is None:
                    self.table.setItem(row, self.COL_POWER, QTableWidgetItem(""))
                    btn_pw = QPushButton()
                    btn_pw.clicked.connect(lambda _=False, r=row: self.toggle_channel_power(r))
                    self.table.setCellWidget(row, self.COL_POWER, btn_pw)
                if not is_caen:
                    btn_pw.setText("Manual Dial")
                    btn_pw.setStyleSheet("background-color: #EFEBE9; color: #5D4037; font-weight: bold;")
                    btn_pw.setEnabled(False)
                else:
                    self._style_power_button(btn_pw, ch.pw)
                    btn_pw.setEnabled(self.connected)
        finally:
            self._updating_table = False

    def _style_fadc_map_combo(self, combo: QComboBox, fadc_ch: int):
        color_hex = OKABE_ITO_FADC_HEX.get(fadc_ch, "#607D8B")
        combo.setStyleSheet(f"font-weight: bold; color: {color_hex};")

    def _style_power_button(self, btn: QPushButton, is_on: bool):
        if is_on:
            btn.setText("Power OFF")
            btn.setStyleSheet("background-color: #009E73; color: white; font-weight: bold;")
        else:
            btn.setText("Power ON")
            btn.setStyleSheet("background-color: #D55E00; color: white; font-weight: bold;")

    def on_fadc_map_changed(self, row: int):
        combo = self.table.cellWidget(row, self.COL_FADC_MAP)
        if combo is None:
            return
        new_map = int(combo.currentData())
        self.channels[row].fadc_map = new_map
        self._style_fadc_map_combo(combo, new_map)
        self._emit_hv_snapshot()

    def on_table_item_changed(self, item: QTableWidgetItem):
        if self._updating_table or item is None:
            return
        row = item.row()
        col = item.column()
        if not (0 <= row < len(self.channels)):
            return
        ch = self.channels[row]
        txt = item.text().strip()

        self._updating_table = True
        try:
            if col == self.COL_SLOT_CH:
                if ":" in txt:
                    s_part, c_part = txt.split(":", 1)
                    try:
                        ch.slot = max(0, int(s_part))
                        ch.hw_ch = max(0, int(c_part))
                    except ValueError:
                        pass
                item.setText(f"{ch.slot}:{ch.hw_ch}")
            elif col == self.COL_VSET:
                try:
                    ch.vset = max(0.0, min(float(txt), ch.svmax, self.current_profile().max_voltage_v))
                except ValueError:
                    pass
                item.setText(f"{ch.vset:.1f}")
            elif col == self.COL_ISET:
                try:
                    ch.iset = max(0.1, min(float(txt), self.current_profile().max_current_ua))
                except ValueError:
                    pass
                item.setText(f"{ch.iset:.1f}")
            elif col == self.COL_RAMP:
                if "/" in txt:
                    u_part, d_part = txt.split("/", 1)
                    try:
                        ch.ramp_up = max(1.0, min(500.0, float(u_part)))
                        ch.ramp_down = max(1.0, min(500.0, float(d_part)))
                    except ValueError:
                        pass
                else:
                    try:
                        val = max(1.0, min(500.0, float(txt)))
                        ch.ramp_up = val
                    except ValueError:
                        pass
                item.setText(f"{ch.ramp_up:.0f}/{ch.ramp_down:.0f}")
            elif col == self.COL_SVMAX:
                try:
                    ch.svmax = max(50.0, min(float(txt), self.current_profile().max_voltage_v))
                    if ch.vset > ch.svmax:
                        ch.vset = ch.svmax
                        vset_item = self.table.item(row, self.COL_VSET)
                        if vset_item:
                            vset_item.setText(f"{ch.vset:.1f}")
                except ValueError:
                    pass
                item.setText(f"{ch.svmax:.0f}")
        finally:
            self._updating_table = False

        self._emit_hv_snapshot()

    def toggle_mode(self, checked: bool):
        self.lbl_analog.setVisible(not checked)
        self.conn_group.setVisible(checked)
        self.caen_fallback_widget.setVisible(checked)
        self.btn_apply.setVisible(checked)
        self.btn_mapped_on.setVisible(checked)
        self.btn_all_off.setVisible(checked)
        self.btn_kill.setVisible(checked)

        self.setup_table_mode(is_caen=checked)

        if checked:
            self.sig_log.emit(
                "<span style='color:#0072B2; font-weight:bold;'>[HV HAL] CAEN Remote Control 활성화됨 "
                "(SY5527 / SY4527LC 서버 및 8-Ch NIM / 탁상형 지원).</span>",
                False,
            )
        else:
            if self.connected:
                self.toggle_caen()
            self.sig_log.emit(
                "<span style='color:#5D4037; font-weight:bold;'>[HV HAL] Analog / Manual 기록 모드로 전환됨.</span>",
                False,
            )
        self._emit_hv_snapshot()

    def _instantiate_backend(self, profile: HvSystemProfile) -> AbstractHvBackend:
        drv_idx = self.combo_driver.currentIndex()
        if drv_idx == 1:
            return CaenHvWrapperBackend(profile)
        if drv_idx == 2:
            return CaenAsciiProtocolBackend(profile)
        if drv_idx == 3:
            return VirtualTwinHvBackend(profile)
        # Auto mode: prefer official CAENHVWrapper if installed, otherwise Digital Twin Simulator
        if CAEN_WRAPPER_AVAILABLE:
            return CaenHvWrapperBackend(profile)
        return VirtualTwinHvBackend(profile)

    def toggle_caen(self):
        if not self.connected:
            prof = self.current_profile()
            link_str = self.combo_link.currentText()
            addr = self.input_ip.text().strip()
            user = self.input_user.text().strip()
            pwd = self.input_pwd.text()

            self.backend = self._instantiate_backend(prof)
            ok, msg = self.backend.connect_device(link_str, addr, user, pwd, self.channels)

            if not ok and self.combo_driver.currentIndex() == 0 and not isinstance(self.backend, VirtualTwinHvBackend):
                self.sig_log.emit(
                    f"<span style='color:#E69F00; font-weight:bold;'>[HV HAL] 실장비 연결 실패 ({msg}) -> "
                    "Digital Twin 시뮬레이터로 자동 전환합니다.</span>",
                    False,
                )
                self.backend = VirtualTwinHvBackend(prof)
                ok, msg = self.backend.connect_device(link_str, addr, user, pwd, self.channels)

            if ok:
                self.connected = True
                self.btn_connect.setText("❌ Disconnect CAEN")
                self.btn_connect.setStyleSheet(
                    "background-color: #607D8B; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;"
                )
                self.caen_fallback_widget.setStyleSheet(
                    "QGroupBox { border: 2px solid #009E73; border-radius: 6px; background-color: #E8F5E9; margin-top: 6px;}"
                )
                self.lbl_fb_notice.setText(
                    f"🟢 [{self.backend.backend_name}] 연결됨 ({self.backend.endpoint_desc}). "
                    "DAQ 탭과 실시간 양방향 제어·모니터링이 동기화됩니다."
                )
                self.lbl_fb_notice.setStyleSheet("color: #009E73; font-weight: bold; font-size: 12px;")

                for row in range(self.num_channels):
                    btn = self.table.cellWidget(row, self.COL_POWER)
                    if btn:
                        btn.setEnabled(True)

                self.sig_log.emit(
                    f"<span style='color:#009E73; font-weight:bold;'>[HV HAL] {msg}</span>",
                    False,
                )
                self.poll_timer.start(500)
            else:
                self.sig_log.emit(
                    f"<span style='color:#D55E00; font-weight:bold;'>[HV ERROR] {msg}</span>",
                    True,
                )
        else:
            self.close_connection()
            self.sig_log.emit(
                "<span style='color:#CC79A7; font-weight:bold;'>[HV HAL] CAEN 세션 연결 해제 및 포트 잠금 반환 완료.</span>",
                False,
            )

        self._emit_hv_snapshot()

    def close_connection(self):
        self.poll_timer.stop()
        if self.backend is not None:
            try:
                self.backend.disconnect_device(self.channels)
            except Exception:
                pass
            self.backend = None
        self.connected = False
        self.btn_connect.setText("🔗 Connect CAEN HV")
        self.btn_connect.setStyleSheet(
            "background-color: #009E73; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px;"
        )
        self.caen_fallback_widget.setStyleSheet(
            "QGroupBox { border: 2px solid #E69F00; border-radius: 6px; background-color: #FFF8E1; margin-top: 6px;}"
        )
        self.lbl_fb_notice.setText(
            "※ 연결 해제됨 (포트 잠금 해제 완료). 표의 VSet/ISet 또는 수동 폴백 입력값이 Run 시작 시 DB에 기록됩니다."
        )
        self.lbl_fb_notice.setStyleSheet("color: #B71C1C; font-weight: bold; font-size: 12px;")

        for row in range(self.num_channels):
            btn = self.table.cellWidget(row, self.COL_POWER)
            if btn:
                btn.setEnabled(False)

    def apply_all_settings(self):
        if not self.connected or self.backend is None:
            self.sig_log.emit(
                "<span style='color:#E69F00; font-weight:bold;'>[HV] 오프라인 상태입니다. 표에 입력된 VSet/ISet 설정이 수동 메타데이터로 보존됩니다.</span>",
                False,
            )
            self._emit_hv_snapshot()
            return

        for ch in self.channels:
            ok, msg = self.backend.apply_channel_settings(ch)
            color = "#0072B2" if ok else "#D55E00"
            self.sig_log.emit(f"<span style='color:{color}; font-weight:bold;'>[HV] {msg}</span>", not ok)

        self.refresh_table_from_state()
        self._emit_hv_snapshot()

    def toggle_channel_power(self, row: int):
        if not (0 <= row < len(self.channels)):
            return
        ch = self.channels[row]
        self.set_channel_power_by_index(row, not ch.pw)

    def set_channel_power_by_index(self, row: int, power_on: bool):
        if not (0 <= row < len(self.channels)):
            return
        if not self.connected or self.backend is None:
            self.sig_log.emit(
                "<span style='color:#D55E00; font-weight:bold;'>[HV] 장비에 먼저 연결(Connect)해 주십시오.</span>",
                True,
            )
            return
        ch = self.channels[row]
        self.backend.apply_channel_settings(ch)
        ok, msg = self.backend.set_channel_power(ch, power_on)
        color = "#009E73" if power_on else "#0072B2"
        if not ok:
            color = "#D55E00"
        self.sig_log.emit(f"<span style='color:{color}; font-weight:bold;'>[HV] {msg}</span>", not ok)
        self.poll_hv_status()

    def set_all_mapped_power(self, power_on: bool):
        if not self.connected or self.backend is None:
            if not self.cb_enable_scm.isChecked():
                self.cb_enable_scm.setChecked(True)
            if not self.connected:
                self.toggle_caen()
        if not self.connected or self.backend is None:
            return
        for row, ch in enumerate(self.channels):
            if ch.fadc_map in (0, 1, 2, 3):
                self.backend.apply_channel_settings(ch)
                self.backend.set_channel_power(ch, power_on)
        self.sig_log.emit(
            f"<span style='color:#009E73; font-weight:bold;'>[HV] FADC Ch 0–3 매핑 고전압 일괄 {'인가(ON)' if power_on else '감압(OFF)'} 명령 전송.</span>",
            False,
        )
        self.poll_hv_status()

    def set_all_channels_power(self, power_on: bool):
        if not self.connected or self.backend is None:
            return
        for ch in self.channels:
            self.backend.set_channel_power(ch, power_on)
        self.poll_hv_status()

    def poll_hv_status(self):
        if not self.connected or self.backend is None:
            return
        self.backend.poll_channels(self.channels)
        self.refresh_table_from_state()
        self._emit_hv_snapshot()

    def refresh_table_from_state(self):
        self._updating_table = True
        try:
            is_caen = self.cb_enable_scm.isChecked()
            for row, ch in enumerate(self.channels):
                vset_it = self.table.item(row, self.COL_VSET)
                if vset_it:
                    vset_it.setText(f"{ch.vset:.1f}")

                vmon_it = self.table.item(row, self.COL_VMON)
                if vmon_it:
                    vmon_it.setText("N/A" if not is_caen else f"{ch.vmon:.2f}")

                imon_it = self.table.item(row, self.COL_IMON)
                if imon_it:
                    imon_it.setText("N/A" if not is_caen else f"{ch.imon:.2f}")

                st_it = self.table.item(row, self.COL_STATE)
                if st_it:
                    st_txt = "MANUAL" if not is_caen else ch.status_text
                    st_it.setText(st_txt)
                    if st_txt in ("TRIP", "OVC", "OVV"):
                        st_it.setForeground(QColor("#D55E00"))
                    elif st_txt.startswith("RAMP"):
                        st_it.setForeground(QColor("#E69F00"))
                    elif st_txt == "ON":
                        st_it.setForeground(QColor("#009E73"))
                    else:
                        st_it.setForeground(QColor("#607D8B"))

                btn = self.table.cellWidget(row, self.COL_POWER)
                if isinstance(btn, QPushButton):
                    self._style_power_button(btn, ch.pw)
        finally:
            self._updating_table = False

    # =========================================================================
    # Bi-Directional Integration API for DaqTab
    # =========================================================================
    def get_fadc_mapped_channels(self) -> Dict[int, HvChannelState]:
        mapped: Dict[int, HvChannelState] = {}
        for ch in self.channels:
            if ch.fadc_map in (0, 1, 2, 3) and ch.fadc_map not in mapped:
                mapped[ch.fadc_map] = ch
        return mapped

    def set_fadc_channel_vset(self, fadc_ch: int, vset_val: float, apply_hw: bool = True):
        mapped = self.get_fadc_mapped_channels()
        ch = mapped.get(fadc_ch)
        if ch is None:
            return
        ch.vset = max(0.0, min(float(vset_val), ch.svmax, self.current_profile().max_voltage_v))
        if apply_hw and self.connected and self.backend is not None:
            ok, msg = self.backend.apply_channel_settings(ch)
            self.sig_log.emit(f"<span style='color:#0072B2; font-weight:bold;'>[DAQ->HV] {msg}</span>", not ok)
        self.refresh_table_from_state()
        self._emit_hv_snapshot()

    def toggle_fadc_channel_power(self, fadc_ch: int):
        mapped = self.get_fadc_mapped_channels()
        ch = mapped.get(fadc_ch)
        if ch is None:
            return
        if not self.cb_enable_scm.isChecked():
            self.cb_enable_scm.setChecked(True)
        if not self.connected:
            self.toggle_caen()
        if self.connected:
            self.set_channel_power_by_index(ch.index, not ch.pw)

    def has_unsafe_channels(self) -> Tuple[bool, str]:
        if not self.cb_enable_scm.isChecked() or not self.connected:
            return False, ""
        mapped = self.get_fadc_mapped_channels()
        issues = []
        for fadc_ch, ch in sorted(mapped.items()):
            if ch.is_tripped:
                issues.append(f"FADC Ch{fadc_ch} (S{ch.slot}:C{ch.hw_ch}) TRIP/OVC 발생")
            elif ch.is_ramping:
                issues.append(f"FADC Ch{fadc_ch} (S{ch.slot}:C{ch.hw_ch}) 전압 램핑 중 ({ch.vmon:.1f}->{ch.vset:.1f}V)")
        return (len(issues) > 0), "; ".join(issues)

    def _emit_hv_snapshot(self):
        prof = self.current_profile()
        mapped = self.get_fadc_mapped_channels()
        fadc_dict = {}
        for fch in range(4):
            c = mapped.get(fch)
            if c is not None:
                fadc_dict[fch] = {
                    "hv_index": c.index,
                    "slot": c.slot,
                    "hw_ch": c.hw_ch,
                    "vset": c.vset,
                    "iset": c.iset,
                    "vmon": c.vmon,
                    "imon": c.imon,
                    "pw": c.pw,
                    "status": c.status_text if self.cb_enable_scm.isChecked() else "MANUAL",
                }
        payload = {
            "caen_enabled": self.cb_enable_scm.isChecked(),
            "connected": self.connected,
            "model": prof.model_id,
            "display_name": prof.display_name,
            "endpoint": self.backend.endpoint_desc if (self.connected and self.backend) else self.input_ip.text(),
            "fadc_channels": fadc_dict,
            "summary": self.get_current_hv(),
        }
        self.sig_hv_updated.emit(payload)

    def get_current_hv(self) -> str:
        mapped = self.get_fadc_mapped_channels()
        if self.cb_enable_scm.isChecked():
            if self.connected:
                parts = []
                for fch in range(4):
                    c = mapped.get(fch)
                    if c:
                        parts.append(f"Ch{fch}:{c.vmon:.1f}V/{c.imon:.2f}uA({c.status_text})")
                return f"{' '.join(parts)} [{self.current_profile().model_id}]"
            v = self.input_caen_fallback_v.text().strip()
            i = self.input_caen_fallback_i.text().strip()
            if v and i:
                return f"{v}V, {i}uA (CAEN Exclusive/Offline Fallback)"
            if v:
                return f"{v}V (CAEN Exclusive/Offline Fallback)"
            parts = [f"Ch{fch}:{mapped[fch].vset:.0f}V" for fch in range(4) if fch in mapped]
            return f"{' '.join(parts)} (CAEN Preset Offline)"

        parts = [f"Ch{fch}:{mapped[fch].vset:.0f}V({mapped[fch].iset:.0f}uA)" for fch in range(4) if fch in mapped]
        return f"{' '.join(parts)} (Analog Manual)"

    def force_shutdown(self):
        if self.connected and self.backend is not None:
            self.sig_log.emit(
                "<span style='color:#D55E00; font-weight:bold;'>[HV EMERGENCY] 전 채널 즉시 차단(KILL/OFF) 신호 전송!</span>",
                True,
            )
            self.backend.emergency_kill_all(self.channels)
            self.refresh_table_from_state()
            self.close_connection()
            self._emit_hv_snapshot()

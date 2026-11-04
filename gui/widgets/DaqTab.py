import os
from datetime import datetime
from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QFont
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)
from core.DatabaseManager import DatabaseManager
from core.Paths import get_bin_path
from core.ProcessManager import ProcessManager
from widgets.path_controller import PathControllerWidget

OKABE_ITO_FADC_COLORS = ("#0072B2", "#D55E00", "#009E73", "#CC79A7")


class DaqTab(QWidget):
    sig_log = Signal(str, bool)
    sig_stat = Signal(dict)
    sig_mode = Signal(str)
    sig_config = Signal(str)
    sig_out_file = Signal(str)
    sig_run_finished = Signal()

    def __init__(self, data_dir, config_dir, hv_provider=None, hv_controller=None):
        super().__init__()
        self.data_dir = data_dir
        self.config_dir = config_dir
        self.hv_provider = hv_provider
        self.hv_controller = hv_controller
        self._syncing_hv_ui = False
        self.start_time = None
        self.auto_mode = "NONE"
        self.abort_requested = False
        self.current_out_file = ""
        self.current_scan_thr = None
        self.scan_step_idx = 0
        self.scan_queue = []

        self.db = DatabaseManager(data_dir=self.data_dir)
        self.last_stats = {"events": 0, "size": 0.0, "rate": 0.0}

        self.daq_manager = ProcessManager(self)

        self.current_subrun = 1
        self.max_subruns = 1

        self.initUI()
        self.connectSignals()

        next_run = self.db.get_latest_run_number()
        self.input_run.setText(next_run)
        QTimer.singleShot(200, self.refresh_config_list)

    def initUI(self):
        layout = QVBoxLayout(self)

        path_layout = QHBoxLayout()
        self.path_data = PathControllerWidget("📁 Data Output Path:", self.data_dir)
        self.path_config = PathControllerWidget("⚙️ Config Input Path:", self.config_dir)
        path_layout.addWidget(self.path_data)
        path_layout.addWidget(self.path_config)
        layout.addLayout(path_layout)

        cfg_group = QGroupBox("Run & Config Settings")
        cfg_layout = QGridLayout()

        cfg_layout.addWidget(QLabel("File Name (Prefix_Run):"), 0, 0)
        prefix_run_lay = QHBoxLayout()
        prefix_run_lay.setContentsMargins(0, 0, 0, 0)
        prefix_run_lay.setSpacing(4)

        self.input_prefix = QLineEdit()
        self.input_prefix.setText("run")
        self.input_prefix.setToolTip("Custom prefix (e.g., laser_1khz, calib_cs137, bg_overnight)")

        self.input_run = QLineEdit()
        self.input_run.setFixedWidth(70)

        prefix_run_lay.addWidget(self.input_prefix, 1)
        prefix_run_lay.addWidget(QLabel("_"), 0)
        prefix_run_lay.addWidget(self.input_run, 0)
        cfg_layout.addLayout(prefix_run_lay, 0, 1)

        cfg_layout.addWidget(QLabel("Hardware Config:"), 1, 0)
        self.combo_cfg = QComboBox()
        cfg_layout.addWidget(self.combo_cfg, 1, 1)

        btn_refresh_cfg = QPushButton("🔄 Refresh Configs")
        btn_refresh_cfg.clicked.connect(self.refresh_config_list)
        cfg_layout.addWidget(btn_refresh_cfg, 1, 2)

        cfg_layout.addWidget(QLabel("Data Quality:"), 2, 0)
        self.combo_qual = QComboBox()
        self.combo_qual.addItems(["🟢 GOOD", "🟡 TEST", "🔴 BAD", "⚪ CALIB"])
        cfg_layout.addWidget(self.combo_qual, 2, 1)

        cfg_layout.addWidget(QLabel("Comments:"), 3, 0)
        self.input_cmt = QLineEdit()
        self.input_cmt.setPlaceholderText("Enter run description...")
        cfg_layout.addWidget(self.input_cmt, 3, 1, 1, 2)

        cfg_group.setLayout(cfg_layout)
        layout.addWidget(cfg_group)

        # =====================================================================
        # Live CAEN High-Voltage Server Link & Quick Control Strip in DAQ Tab
        # =====================================================================
        self.hv_quick_group = QGroupBox(
            "⚡ High-Voltage Server Link & FADC Ch 0–3 Live Control (CAEN SY5527 / SY4527LC / 8-Ch NIM)"
        )
        self.hv_quick_group.setStyleSheet(
            "QGroupBox { border: 2px solid #0072B2; border-radius: 6px; background-color: #F4F9FD; margin-top: 8px;} "
            "QGroupBox::title { color: #0072B2; font-weight: bold; }"
        )
        hv_vbox = QVBoxLayout(self.hv_quick_group)
        hv_vbox.setContentsMargins(8, 16, 8, 8)
        hv_vbox.setSpacing(6)

        hv_top_bar = QHBoxLayout()
        self.lbl_daq_hv_status = QLabel("⚪ HV Mode: Analog / Manual")
        self.lbl_daq_hv_status.setStyleSheet("color: #37474F; font-weight: bold; font-size: 12px;")

        self.cb_hv_interlock = QCheckBox("🔒 HV Safety Interlock (Ramping/Trip 시 Run 시작 보호)")
        self.cb_hv_interlock.setChecked(True)
        self.cb_hv_interlock.setStyleSheet("color: #0072B2; font-weight: bold; font-size: 12px;")

        self.btn_daq_hv_conn = QPushButton("🔗 Connect Server")
        self.btn_daq_hv_conn.setStyleSheet(
            "background-color: #0072B2; color: white; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
        )
        self.btn_daq_hv_conn.clicked.connect(self.on_daq_hv_connect_clicked)

        self.btn_daq_hv_apply = QPushButton("⚡ Apply VSet")
        self.btn_daq_hv_apply.setStyleSheet(
            "background-color: #E69F00; color: black; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
        )
        self.btn_daq_hv_apply.clicked.connect(self.on_daq_hv_apply_clicked)

        self.btn_daq_hv_all_on = QPushButton("🟢 Ch 0–3 ON")
        self.btn_daq_hv_all_on.setStyleSheet(
            "background-color: #009E73; color: white; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
        )
        self.btn_daq_hv_all_on.clicked.connect(lambda: self.on_daq_hv_all_power(True))

        self.btn_daq_hv_all_off = QPushButton("🔴 Ch 0–3 OFF")
        self.btn_daq_hv_all_off.setStyleSheet(
            "background-color: #D55E00; color: white; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
        )
        self.btn_daq_hv_all_off.clicked.connect(lambda: self.on_daq_hv_all_power(False))

        hv_top_bar.addWidget(self.lbl_daq_hv_status, stretch=2)
        hv_top_bar.addWidget(self.cb_hv_interlock)
        hv_top_bar.addWidget(self.btn_daq_hv_conn)
        hv_top_bar.addWidget(self.btn_daq_hv_apply)
        hv_top_bar.addWidget(self.btn_daq_hv_all_on)
        hv_top_bar.addWidget(self.btn_daq_hv_all_off)
        hv_vbox.addLayout(hv_top_bar)

        hv_ch_grid = QGridLayout()
        hv_ch_grid.setSpacing(6)
        self.daq_hv_lbl_map = []
        self.daq_hv_spin_vset = []
        self.daq_hv_lbl_mon = []
        self.daq_hv_btn_pw = []

        for fch in range(4):
            c_hex = OKABE_ITO_FADC_COLORS[fch]
            card = QWidget()
            card.setStyleSheet(
                f"QWidget {{ background-color: #FFFFFF; border: 1px solid {c_hex}; border-radius: 4px; }}"
            )
            card_lay = QHBoxLayout(card)
            card_lay.setContentsMargins(6, 4, 6, 4)
            card_lay.setSpacing(4)

            lbl_ch = QLabel(f"Ch{fch} [S0:C{fch}]")
            lbl_ch.setStyleSheet(f"color: {c_hex}; font-weight: bold; font-size: 12px; border: none;")

            sp_v = QDoubleSpinBox()
            sp_v.setRange(0.0, 8000.0)
            sp_v.setDecimals(1)
            sp_v.setSingleStep(10.0)
            sp_v.setValue(1700.0)
            sp_v.setSuffix(" V")
            sp_v.setFixedWidth(92)
            sp_v.setStyleSheet("font-weight: bold; font-size: 12px;")
            sp_v.valueChanged.connect(lambda val, ch_idx=fch: self.on_daq_vset_edited(ch_idx, val))

            lbl_mon = QLabel("VMon: N/A")
            lbl_mon.setFont(QFont("Consolas", 10, QFont.Bold))
            lbl_mon.setStyleSheet("color: #455A64; border: none;")

            btn_pw = QPushButton("MANUAL")
            btn_pw.setFixedWidth(68)
            btn_pw.setStyleSheet("background-color: #90A4AE; color: white; font-weight: bold; font-size: 11px;")
            btn_pw.clicked.connect(lambda _=False, ch_idx=fch: self.on_daq_ch_power_clicked(ch_idx))

            card_lay.addWidget(lbl_ch)
            card_lay.addWidget(sp_v)
            card_lay.addWidget(lbl_mon, stretch=1)
            card_lay.addWidget(btn_pw)

            self.daq_hv_lbl_map.append(lbl_ch)
            self.daq_hv_spin_vset.append(sp_v)
            self.daq_hv_lbl_mon.append(lbl_mon)
            self.daq_hv_btn_pw.append(btn_pw)
            hv_ch_grid.addWidget(card, 0, fch)

        hv_vbox.addLayout(hv_ch_grid)
        layout.addWidget(self.hv_quick_group)

        control_layout = QHBoxLayout()

        std_group = QGroupBox("🕹️ Manual / Long-Term DAQ")
        std_group.setStyleSheet(
            "QGroupBox { border: 2px solid #4CAF50; border-radius: 6px; background-color: #F1F8E9;} QGroupBox::title { color: #2E7D32; }"
        )
        std_lay = QVBoxLayout()

        mode_lay = QHBoxLayout()
        mode_lay.addWidget(QLabel("Run Mode:"))
        self.combo_run_mode = QComboBox()
        self.combo_run_mode.addItems(
            ["Continuous (무한 수집)", "Target Events (이벤트 종료)", "Target Time (시간 종료)", "Long-Term (다중 분할 런)"]
        )
        self.combo_run_mode.currentIndexChanged.connect(self.update_std_ui)
        mode_lay.addWidget(self.combo_run_mode)
        std_lay.addLayout(mode_lay)

        self.widget_val = QWidget()
        val_lay = QHBoxLayout(self.widget_val)
        val_lay.setContentsMargins(0, 0, 0, 0)
        self.lbl_target = QLabel("Target Limit:")
        self.spin_target = QSpinBox()
        self.spin_target.setRange(1, 100000000)
        self.spin_target.setValue(10000)
        val_lay.addWidget(self.lbl_target)
        val_lay.addWidget(self.spin_target)
        std_lay.addWidget(self.widget_val)

        self.widget_sub = QWidget()
        sub_lay = QHBoxLayout(self.widget_sub)
        sub_lay.setContentsMargins(0, 0, 0, 0)
        self.spin_sub_max = QSpinBox()
        self.spin_sub_max.setRange(1, 9999)
        self.spin_sub_max.setValue(5)
        self.spin_idle = QSpinBox()
        self.spin_idle.setRange(0, 3600)
        self.spin_idle.setValue(5)
        sub_lay.addWidget(QLabel("Total Chunks:"))
        sub_lay.addWidget(self.spin_sub_max)
        sub_lay.addWidget(QLabel("Idle(s):"))
        sub_lay.addWidget(self.spin_idle)
        std_lay.addWidget(self.widget_sub)
        std_lay.addStretch()

        self.btn_start_man = QPushButton("▶ START MANUAL DAQ")
        self.btn_start_man.setStyleSheet(
            "background-color: #4CAF50; color: white; padding: 12px; font-weight:bold; font-size:14px;"
        )
        self.btn_stop_man = QPushButton("⏹ STOP DAQ / SCAN")
        self.btn_stop_man.setStyleSheet(
            "background-color: #F44336; color: white; padding: 12px; font-weight:bold; font-size:14px;"
        )
        self.btn_stop_man.setEnabled(False)
        std_lay.addWidget(self.btn_start_man)
        std_lay.addWidget(self.btn_stop_man)
        std_group.setLayout(std_lay)
        control_layout.addWidget(std_group)

        scan_group = QGroupBox("🔄 Threshold (ADC) Auto Scan")
        scan_group.setStyleSheet(
            "QGroupBox { border: 2px solid #FF9800; border-radius: 6px; background-color: #FFF3E0;} QGroupBox::title { color: #E65100; }"
        )
        scan_lay = QVBoxLayout()

        scan_grid = QGridLayout()
        self.sp_start = QSpinBox()
        self.sp_start.setRange(1, 4095)
        self.sp_start.setValue(20)
        self.sp_end = QSpinBox()
        self.sp_end.setRange(1, 4095)
        self.sp_end.setValue(100)
        self.sp_step = QSpinBox()
        self.sp_step.setRange(1, 500)
        self.sp_step.setValue(10)
        scan_grid.addWidget(QLabel("Start ADC:"), 0, 0)
        scan_grid.addWidget(self.sp_start, 0, 1)
        scan_grid.addWidget(QLabel("End ADC:"), 0, 2)
        scan_grid.addWidget(self.sp_end, 0, 3)
        scan_grid.addWidget(QLabel("Step ADC:"), 1, 0)
        scan_grid.addWidget(self.sp_step, 1, 1)
        scan_lay.addLayout(scan_grid)

        scan_opt = QHBoxLayout()
        self.combo_scan_mode = QComboBox()
        self.combo_scan_mode.addItems(["Time (sec)", "Events"])
        self.spin_scan_val = QSpinBox()
        self.spin_scan_val.setRange(1, 10000000)
        self.spin_scan_val.setValue(10)
        self.spin_scan_idle = QSpinBox()
        self.spin_scan_idle.setRange(0, 3600)
        self.spin_scan_idle.setValue(3)
        scan_opt.addWidget(self.combo_scan_mode)
        scan_opt.addWidget(self.spin_scan_val)
        scan_opt.addWidget(QLabel("Idle(s):"))
        scan_opt.addWidget(self.spin_scan_idle)
        scan_lay.addLayout(scan_opt)
        scan_lay.addStretch()

        self.btn_scan = QPushButton("🔄 START THRESHOLD SCAN")
        self.btn_scan.setStyleSheet(
            "background-color: #FF9800; color: white; padding: 12px; font-weight:bold; font-size:14px;"
        )
        scan_lay.addWidget(self.btn_scan)
        scan_group.setLayout(scan_lay)
        control_layout.addWidget(scan_group)

        layout.addLayout(control_layout)
        self.update_std_ui()
        layout.addStretch()

    def update_std_ui(self):
        idx = self.combo_run_mode.currentIndex()
        if idx == 0:
            self.widget_val.setVisible(False)
            self.widget_sub.setVisible(False)
        elif idx == 1:
            self.widget_val.setVisible(True)
            self.widget_sub.setVisible(False)
            self.lbl_target.setText("Target Events:")
        elif idx == 2:
            self.widget_val.setVisible(True)
            self.widget_sub.setVisible(False)
            self.lbl_target.setText("Target Time (sec):")
        elif idx == 3:
            self.widget_val.setVisible(True)
            self.widget_sub.setVisible(True)
            self.lbl_target.setText("Per Chunk Time (sec):")

    def refresh_config_list(self):
        self.combo_cfg.clear()
        current_cfg_dir = self.path_config.get_path()

        if not os.path.exists(current_cfg_dir):
            self.sig_log.emit(f"\033[1;31m[ERROR] Config 경로를 찾을 수 없습니다: {current_cfg_dir}\033[0m", True)
            return

        cfg_files = [
            f for f in os.listdir(current_cfg_dir) if f.lower().endswith(".config") or f.lower().endswith(".cfg")
        ]

        if not cfg_files:
            self.sig_log.emit(f"\033[1;33m[WARNING] '{current_cfg_dir}' 내에 설정 파일(*.cfg)이 없습니다.\033[0m", False)
            return

        for f in sorted(cfg_files):
            self.combo_cfg.addItem(f, os.path.join(current_cfg_dir, f))

        self.sig_log.emit(f"\033[1;32m[SYSTEM] {len(cfg_files)}개의 하드웨어 설정 파일을 로드했습니다.\033[0m", False)

    def connectSignals(self):
        self.btn_start_man.clicked.connect(lambda: self.start_manual(1))
        self.btn_stop_man.clicked.connect(self.stop_daq)
        self.btn_scan.clicked.connect(self.start_scan)

        self.daq_manager.log_signal.connect(lambda msg, is_err: self.sig_log.emit(msg, is_err))
        self.daq_manager.stat_signal.connect(self.update_stats)
        self.daq_manager.state_signal.connect(self.handle_process_state)

        if self.hv_controller is not None and hasattr(self.hv_controller, "sig_hv_updated"):
            self.hv_controller.sig_hv_updated.connect(self.update_hv_panel)
            self.hv_controller._emit_hv_snapshot()

    def update_hv_panel(self, snap: dict):
        self._syncing_hv_ui = True
        try:
            caen_en = bool(snap.get("caen_enabled", False))
            conn = bool(snap.get("connected", False))
            disp = snap.get("display_name", "CAEN HV")
            ep = snap.get("endpoint", "")

            if not caen_en:
                self.lbl_daq_hv_status.setText("⚪ HV Mode: Analog / Manual (수동 기록 연동)")
                self.lbl_daq_hv_status.setStyleSheet("color: #5D4037; font-weight: bold; font-size: 12px;")
                self.btn_daq_hv_conn.setText("🔗 Enable & Connect")
                self.btn_daq_hv_conn.setStyleSheet(
                    "background-color: #0072B2; color: white; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
                )
            elif conn:
                self.lbl_daq_hv_status.setText(f"🟢 [{disp}] Online ({ep})")
                self.lbl_daq_hv_status.setStyleSheet("color: #009E73; font-weight: bold; font-size: 12px;")
                self.btn_daq_hv_conn.setText("❌ Disconnect")
                self.btn_daq_hv_conn.setStyleSheet(
                    "background-color: #607D8B; color: white; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
                )
            else:
                self.lbl_daq_hv_status.setText(f"🟡 [{disp}] Offline / Exclusive-Lock Fallback")
                self.lbl_daq_hv_status.setStyleSheet("color: #D55E00; font-weight: bold; font-size: 12px;")
                self.btn_daq_hv_conn.setText("🔗 Connect Server")
                self.btn_daq_hv_conn.setStyleSheet(
                    "background-color: #0072B2; color: white; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
                )

            fadc_chs = snap.get("fadc_channels", {})
            for fch in range(4):
                info = fadc_chs.get(fch)
                if info is None:
                    self.daq_hv_lbl_map[fch].setText(f"Ch{fch} [Unmapped]")
                    self.daq_hv_lbl_mon[fch].setText("VMon: --")
                    self.daq_hv_btn_pw[fch].setText("N/A")
                    self.daq_hv_btn_pw[fch].setEnabled(False)
                    continue

                slot = info.get("slot", 0)
                hw_ch = info.get("hw_ch", fch)
                vset = float(info.get("vset", 1700.0))
                vmon = float(info.get("vmon", 0.0))
                imon = float(info.get("imon", 0.0))
                st = str(info.get("status", "OFF"))

                self.daq_hv_lbl_map[fch].setText(f"Ch{fch} [S{slot}:C{hw_ch}]")
                if not self.daq_hv_spin_vset[fch].hasFocus():
                    self.daq_hv_spin_vset[fch].setValue(vset)

                if caen_en and conn:
                    self.daq_hv_lbl_mon[fch].setText(f"{vmon:.1f}V | {imon:.2f}uA")
                else:
                    self.daq_hv_lbl_mon[fch].setText(f"Set:{vset:.0f}V")

                btn = self.daq_hv_btn_pw[fch]
                btn.setEnabled(True)
                btn.setText(st)
                if st in ("TRIP", "OVC", "OVV"):
                    btn.setStyleSheet("background-color: #D55E00; color: white; font-weight: bold; font-size: 11px;")
                elif st.startswith("RAMP"):
                    btn.setStyleSheet("background-color: #E69F00; color: black; font-weight: bold; font-size: 11px;")
                elif st == "ON":
                    btn.setStyleSheet("background-color: #009E73; color: white; font-weight: bold; font-size: 11px;")
                else:
                    btn.setStyleSheet("background-color: #607D8B; color: white; font-weight: bold; font-size: 11px;")
        finally:
            self._syncing_hv_ui = False

        cfg_file = self.combo_cfg.currentData()
        if cfg_file:
            self.sig_config.emit(self.get_config_summary(cfg_file, thr_override=self.current_scan_thr))

    def on_daq_hv_connect_clicked(self):
        if self.hv_controller is None:
            return
        if not self.hv_controller.cb_enable_scm.isChecked():
            self.hv_controller.cb_enable_scm.setChecked(True)
        self.hv_controller.toggle_caen()

    def on_daq_hv_apply_clicked(self):
        if self.hv_controller is None:
            return
        for fch in range(4):
            self.hv_controller.set_fadc_channel_vset(fch, self.daq_hv_spin_vset[fch].value(), apply_hw=False)
        self.hv_controller.apply_all_settings()

    def on_daq_hv_all_power(self, power_on: bool):
        if self.hv_controller is None:
            return
        self.hv_controller.set_all_mapped_power(power_on)

    def on_daq_vset_edited(self, fadc_ch: int, val: float):
        if self._syncing_hv_ui or self.hv_controller is None:
            return
        self.hv_controller.set_fadc_channel_vset(fadc_ch, val, apply_hw=False)

    def on_daq_ch_power_clicked(self, fadc_ch: int):
        if self.hv_controller is None:
            return
        self.hv_controller.set_fadc_channel_vset(fadc_ch, self.daq_hv_spin_vset[fadc_ch].value(), apply_hw=True)
        self.hv_controller.toggle_fadc_channel_power(fadc_ch)

    def _check_hv_interlock(self) -> bool:
        if not self.cb_hv_interlock.isChecked() or self.hv_controller is None:
            return True
        unsafe, reason = self.hv_controller.has_unsafe_channels()
        if not unsafe:
            return True
        reply = QMessageBox.warning(
            self,
            "HV Safety Interlock Triggered",
            f"고전압 채널이 아직 안정화되지 않았거나 Trip 상태입니다:\n\n• {reason}\n\n"
            "전압 안정화 후 시작하는 것을 권장합니다. 그래도 강제로 DAQ Run을 시작하시겠습니까?",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        return reply == QMessageBox.Yes

    def generate_out_filename(self, subrun=0):
        prefix = self.input_prefix.text().strip() or "run"
        run_num = self.input_run.text().strip() or "999"
        target_dir = self.path_data.get_path()
        os.makedirs(target_dir, exist_ok=True)

        if self.auto_mode == "MANUAL" and self.max_subruns > 1:
            fname = f"{prefix}_{run_num}_{subrun:03d}.dat"
        else:
            fname = f"{prefix}_{run_num}.dat"
        return os.path.join(target_dir, fname)

    def get_config_summary(self, cfg_path, thr_override=None):
        params = {}
        try:
            with open(cfg_path, "r", encoding="utf-8") as f:
                for line in f:
                    clean_line = line.split("#")[0].strip()
                    if not clean_line:
                        continue
                    parts = clean_line.split()
                    if len(parts) >= 2:
                        params[parts[0]] = " ".join(parts[1:])
        except Exception:
            return "Config file read error."

        if not params:
            return "No valid parameters found."

        thr_disp = f"{thr_override} (Scan Override)" if thr_override is not None else params.get("THR", "-")
        hv_disp = self.hv_provider() if callable(self.hv_provider) else "-"

        return f"""
        <table style='width:100%; font-size:12px; color:#424242;'>
            <tr>
                <td style='padding:2px;'><b>RL:</b> {params.get('RECORD_LEN', '-')}</td>
                <td style='padding:2px;'><b>TLT:</b> {params.get('TRIG_TLT', '-')}</td>
            </tr>
            <tr>
                <td style='padding:2px;'><b>POL:</b> {params.get('POL', '-')}</td>
                <td style='padding:2px;'><b>CW:</b> {params.get('CW', '-')}</td>
            </tr>
            <tr>
                <td style='padding:2px;'><b>DLY:</b> {params.get('DLY', '-')}</td>
                <td style='padding:2px;'><b>OFF:</b> {params.get('DACOFF', '-')}</td>
            </tr>
            <tr>
                <td colspan='2' style='padding:2px; color:#D32F2F;'><b>THR:</b> {thr_disp}</td>
            </tr>
            <tr>
                <td colspan='2' style='padding:2px; color:#1565C0;'><b>HV:</b> {hv_disp}</td>
            </tr>
        </table>
        """

    def start_manual(self, subrun_idx=1):
        cfg_file = self.combo_cfg.currentData()
        if not cfg_file:
            self.sig_log.emit("\033[1;31m[ERROR] 설정 파일을 찾을 수 없습니다!\033[0m", True)
            return
        if subrun_idx == 1 and not self._check_hv_interlock():
            return

        self.abort_requested = False
        self.auto_mode = "MANUAL"
        self.current_scan_thr = None
        idx = self.combo_run_mode.currentIndex()
        if subrun_idx == 1:
            self.max_subruns = self.spin_sub_max.value() if idx == 3 else 1

        self.current_subrun = subrun_idx
        self.current_out_file = self.generate_out_filename(subrun_idx)
        self.last_stats = {"events": 0, "size": 0.0, "rate": 0.0}

        bin_path = get_bin_path("frontend_nkfadc500")
        args = ["-f", cfg_file, "-o", self.current_out_file]

        if idx == 1:
            args.extend(["-n", str(self.spin_target.value())])
        elif idx in [2, 3]:
            args.extend(["-t", str(self.spin_target.value())])

        self.btn_start_man.setEnabled(False)
        self.btn_scan.setEnabled(False)
        self.btn_stop_man.setEnabled(True)
        self.start_time = datetime.now()

        prefix = self.input_prefix.text().strip() or "run"
        run_str = (
            f"{prefix}_{self.input_run.text()}_{self.current_subrun:03d}"
            if self.max_subruns > 1
            else f"{prefix}_{self.input_run.text()}"
        )
        self.sig_out_file.emit(self.current_out_file)
        self.sig_mode.emit(f"RUN [{run_str}]")
        self.sig_config.emit(self.get_config_summary(cfg_file))

        self.sig_log.emit("\033[1;36m[SYSTEM] Starting DAQ...\033[0m", False)
        self.daq_manager.start_process(bin_path, args)

    def start_scan(self):
        cfg_file = self.combo_cfg.currentData()
        if not cfg_file:
            self.sig_log.emit("\033[1;31m[ERROR] 설정 파일을 찾을 수 없습니다!\033[0m", True)
            return

        if self.sp_start.value() > self.sp_end.value():
            self.sig_log.emit("\033[1;31m[ERROR] Start ADC must be <= End ADC!\033[0m", True)
            return
        if not self._check_hv_interlock():
            return

        self.abort_requested = False
        self.auto_mode = "SCAN"
        self.scan_step_idx = 0
        self.scan_queue = list(range(self.sp_start.value(), self.sp_end.value() + 1, self.sp_step.value()))
        self.btn_start_man.setEnabled(False)
        self.btn_scan.setEnabled(False)
        self.btn_stop_man.setEnabled(True)
        self.run_scan_step()

    def run_scan_step(self):
        if self.abort_requested or not self.scan_queue:
            self.auto_mode = "NONE"
            self.current_scan_thr = None
            self.sig_mode.emit("IDLE")
            self.btn_start_man.setEnabled(True)
            self.btn_scan.setEnabled(True)
            self.btn_stop_man.setEnabled(False)
            if not self.abort_requested:
                self.sig_log.emit("\033[1;32m[AUTO] All threshold scan steps completed.\033[0m", False)
                if self.input_run.text().isdigit():
                    self.input_run.setText(str(int(self.input_run.text()) + 1))
            return

        curr_thr = self.scan_queue.pop(0)
        self.scan_step_idx += 1
        self.current_scan_thr = curr_thr
        self.start_time = datetime.now()
        self.last_stats = {"events": 0, "size": 0.0, "rate": 0.0}

        target_dir = self.path_data.get_path()
        os.makedirs(target_dir, exist_ok=True)
        prefix = self.input_prefix.text().strip() or "run"
        self.current_out_file = os.path.join(target_dir, f"{prefix}_{self.input_run.text()}_thr{curr_thr}.dat")

        self.sig_out_file.emit(self.current_out_file)
        self.sig_mode.emit(f"SCAN [THR={curr_thr}]")
        self.sig_config.emit(self.get_config_summary(self.combo_cfg.currentData(), thr_override=curr_thr))

        bin_path = get_bin_path("frontend_nkfadc500")
        # Pass --thr <curr_thr> so hardware threshold is genuinely updated on every scan step!
        args = ["-f", self.combo_cfg.currentData(), "-o", self.current_out_file, "--thr", str(curr_thr)]
        if self.combo_scan_mode.currentIndex() == 0:
            args.extend(["-t", str(self.spin_scan_val.value())])
        else:
            args.extend(["-n", str(self.spin_scan_val.value())])

        self.sig_log.emit(f"\033[1;36m[SYSTEM] Starting Scan Step #{self.scan_step_idx} (THR={curr_thr} ADC)...\033[0m", False)
        self.daq_manager.start_process(bin_path, args)

    def handle_process_state(self, is_running):
        self.combo_cfg.setEnabled(not is_running)
        self.input_run.setEnabled(not is_running)
        self.input_prefix.setEnabled(not is_running)

        if not is_running and self.start_time is not None:
            end_time = datetime.now()
            elapsed = (end_time - self.start_time).total_seconds()

            qual_parts = self.combo_qual.currentText().split(" ")
            quality_text = qual_parts[1] if len(qual_parts) > 1 else qual_parts[0]
            cfg_file = self.combo_cfg.currentData()
            cfg_name = os.path.basename(cfg_file) if cfg_file else "Unknown"

            subrun_id = self.current_subrun if self.auto_mode == "MANUAL" else self.scan_step_idx
            base_cmt = self.input_cmt.text().strip()
            extra_tags = []
            if self.current_scan_thr is not None:
                extra_tags.append(f"THR={self.current_scan_thr}")
            if callable(self.hv_provider):
                hv_info = self.hv_provider()
                if hv_info and not hv_info.startswith("N/A"):
                    extra_tags.append(f"HV={hv_info}")
            full_comment = f"{base_cmt} [{' | '.join(extra_tags)}]".strip() if extra_tags else base_cmt

            self.db.db_dir = self.path_data.get_path()
            self.db.db_path = os.path.join(self.db.db_dir, "fadc500_history.db")
            self.db.init_tables()
            self.db.insert_frontend_summary(
                run_num=int(self.input_run.text()) if self.input_run.text().isdigit() else 0,
                subrun=subrun_id,
                start=self.start_time.strftime("%Y-%m-%d %H:%M:%S"),
                end=end_time.strftime("%Y-%m-%d %H:%M:%S"),
                elapsed=round(elapsed, 2),
                events=self.last_stats.get("events", 0),
                size_mb=float(self.last_stats.get("size", 0.0)),
                rate=float(self.last_stats.get("rate", 0.0)),
                mode=self.auto_mode,
                quality=quality_text,
                comments=full_comment,
                config_file=cfg_name,
            )
            self.sig_log.emit("\033[1;32m[DB] Run summary safely stored to SQLite database.\033[0m", False)
            self.sig_run_finished.emit()
            self.start_time = None

            if not self.abort_requested and self.auto_mode == "MANUAL" and self.current_subrun < self.max_subruns:
                idle = self.spin_idle.value()
                self.sig_mode.emit(f"IDLE ({idle}s)")
                QTimer.singleShot(idle * 1000, lambda: self.start_manual(self.current_subrun + 1))
            elif not self.abort_requested and self.auto_mode == "SCAN":
                idle = self.spin_scan_idle.value()
                self.sig_mode.emit(f"IDLE ({idle}s)")
                QTimer.singleShot(idle * 1000, self.run_scan_step)
            else:
                self.auto_mode = "NONE"
                self.current_scan_thr = None
                self.sig_mode.emit("IDLE")

                if not self.abort_requested and self.input_run.text().isdigit():
                    self.input_run.setText(str(int(self.input_run.text()) + 1))

                self.btn_start_man.setEnabled(True)
                self.btn_scan.setEnabled(True)
                self.btn_stop_man.setEnabled(False)

    def update_stats(self, stat_dict):
        self.last_stats.update(stat_dict)
        self.sig_stat.emit(stat_dict)
        if "log" in stat_dict:
            self.sig_log.emit(stat_dict["log"], stat_dict.get("is_error", False))

    def stop_daq(self):
        self.abort_requested = True
        self.scan_queue.clear()
        self.sig_log.emit("\033[1;33m[GUI] Stopping DAQ / cancelling remaining queue...\033[0m", False)
        self.daq_manager.stop_process()
        self.btn_start_man.setEnabled(True)
        self.btn_scan.setEnabled(True)
        self.btn_stop_man.setEnabled(False)

    def force_abort(self):
        self.abort_requested = True
        self.auto_mode = "NONE"
        self.scan_queue.clear()
        self.daq_manager.stop_process()
        self.btn_start_man.setEnabled(True)
        self.btn_scan.setEnabled(True)
        self.btn_stop_man.setEnabled(False)

    def is_running(self):
        return self.daq_manager.is_running() or self.start_time is not None

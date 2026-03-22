import os
from datetime import datetime
from core.DatabaseManager import DatabaseManager
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
                             QLabel, QGroupBox, QLineEdit, QComboBox, QSpinBox, 
                             QRadioButton, QButtonGroup, QTabWidget, QFormLayout)
from PyQt5.QtCore import Qt, pyqtSignal, QTimer, QProcess
from core.ProcessManager import ProcessManager

class DaqTab(QWidget):
    sig_log = pyqtSignal(str, bool)
    sig_stat = pyqtSignal(dict)
    sig_mode = pyqtSignal(str)
    sig_config = pyqtSignal(str) 

    def __init__(self, data_dir):
        super().__init__()
        self.data_dir = data_dir
        self.start_time = None
        self.auto_mode = "NONE"
        self.scan_queue = []
        
        self.db = DatabaseManager()
        self.last_stats = {'events': 0, 'size': 0.0, 'rate': 0.0}
        
        self.daq_manager = ProcessManager()
        self.mon_process = QProcess()
        
        self.current_subrun = 1
        self.max_subruns = 1
        
        self.initUI()
        self.connectSignals()

    def initUI(self):
        layout = QVBoxLayout(self)
        
        info_group = QGroupBox("Run Information & Meta Data")
        info_layout = QHBoxLayout()
        self.input_run = QLineEdit("101")
        self.input_run.setFixedWidth(80)
        self.combo_qual = QComboBox(); self.combo_qual.addItems(["🟢 GOOD", "🟡 CALIBRATION", "🔴 BAD"])
        self.input_cmt = QLineEdit(); self.input_cmt.setPlaceholderText("샘플 특이사항, HV 인가값 등 기록...")
        self.combo_cfg = QComboBox(); self.combo_cfg.addItem("settings.cfg", os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../config/settings.cfg")))
        
        info_layout.addWidget(QLabel("Run:")); info_layout.addWidget(self.input_run)
        info_layout.addWidget(QLabel("Config:")); info_layout.addWidget(self.combo_cfg)
        info_layout.addWidget(QLabel("Quality:")); info_layout.addWidget(self.combo_qual)
        info_layout.addWidget(QLabel("Comment:")); info_layout.addWidget(self.input_cmt, stretch=1)
        info_group.setLayout(info_layout); layout.addWidget(info_group)

        self.sub_tabs = QTabWidget()
        
        tab_manual = QWidget()
        man_layout = QVBoxLayout(tab_manual)

        cond_layout = QHBoxLayout()
        cond_group = QGroupBox("Stop Condition (1개 파일 기준)")
        c_lay = QHBoxLayout()
        self.rb_cont = QRadioButton("Continuous"); self.rb_evt = QRadioButton("By Events"); self.rb_time = QRadioButton("By Time (s)")
        self.rb_cont.setChecked(True)
        self.spin_val = QSpinBox(); self.spin_val.setRange(1, 10000000); self.spin_val.setValue(10000); self.spin_val.setEnabled(False)
        self.rb_cont.toggled.connect(lambda: self.spin_val.setEnabled(not self.rb_cont.isChecked()))
        c_lay.addWidget(self.rb_cont); c_lay.addWidget(self.rb_evt); c_lay.addWidget(self.rb_time); c_lay.addWidget(self.spin_val)
        cond_group.setLayout(c_lay); cond_layout.addWidget(cond_group, stretch=2)

        sub_group = QGroupBox("Long-Term Sub-run")
        s_lay = QHBoxLayout()
        self.sp_sub_max = QSpinBox(); self.sp_sub_max.setRange(1, 9999); self.sp_sub_max.setValue(1)
        self.sp_idle = QSpinBox(); self.sp_idle.setRange(0, 3600); self.sp_idle.setValue(5)
        s_lay.addWidget(QLabel("Max Files:")); s_lay.addWidget(self.sp_sub_max)
        s_lay.addWidget(QLabel("Idle(s):")); s_lay.addWidget(self.sp_idle)
        sub_group.setLayout(s_lay); cond_layout.addWidget(sub_group, stretch=1)
        man_layout.addLayout(cond_layout)

        self.btn_start = QPushButton("▶ START MANUAL / LONG-TERM DAQ")
        self.btn_start.setStyleSheet("background-color: #4CAF50; color: white; padding: 15px; font-weight: bold; font-size: 14px;")
        man_layout.addWidget(self.btn_start); man_layout.addStretch()

        tab_scan = QWidget(); scan_layout = QVBoxLayout(tab_scan)
        scan_row = QHBoxLayout()
        self.sp_start = QSpinBox(); self.sp_start.setRange(10, 1000); self.sp_start.setValue(50)
        self.sp_end = QSpinBox(); self.sp_end.setRange(10, 1000); self.sp_end.setValue(150)
        self.sp_step = QSpinBox(); self.sp_step.setRange(1, 100); self.sp_step.setValue(10)
        self.sp_scan_time = QSpinBox(); self.sp_scan_time.setRange(1, 3600); self.sp_scan_time.setValue(10)
        self.sp_scan_idle = QSpinBox(); self.sp_scan_idle.setRange(0, 3600); self.sp_scan_idle.setValue(2)
        
        scan_row.addWidget(QLabel("Start:")); scan_row.addWidget(self.sp_start)
        scan_row.addWidget(QLabel("End:")); scan_row.addWidget(self.sp_end)
        scan_row.addWidget(QLabel("Step:")); scan_row.addWidget(self.sp_step)
        scan_row.addWidget(QLabel("Time(s):")); scan_row.addWidget(self.sp_scan_time)
        scan_row.addWidget(QLabel("Idle(s):")); scan_row.addWidget(self.sp_scan_idle)
        scan_layout.addLayout(scan_row)
        
        self.btn_scan = QPushButton("🔄 START THRESHOLD SCAN")
        self.btn_scan.setStyleSheet("background-color: #FF9800; color: white; padding: 15px; font-weight: bold; font-size: 14px;")
        scan_layout.addWidget(self.btn_scan); scan_layout.addStretch()

        self.sub_tabs.addTab(tab_manual, "🕹️ Standard DAQ")
        self.sub_tabs.addTab(tab_scan, "🔄 THR Scan")
        layout.addWidget(self.sub_tabs)

    def connectSignals(self):
        self.btn_start.clicked.connect(self.start_manual)
        self.btn_scan.clicked.connect(self.start_scan)
        self.daq_manager.log_signal.connect(lambda msg: self.sig_log.emit(msg, False))
        self.daq_manager.stat_signal.connect(self.update_stats_and_emit)
        self.daq_manager.state_signal.connect(self.handle_process_state)

    def update_stats_and_emit(self, stats):
        self.last_stats.update(stats)
        self.sig_stat.emit(stats)

    def is_running(self):
        return self.daq_manager.process.state() == QProcess.Running

    # 💡 [UX 신규 패치] 주석을 제거하고 깔끔한 2열 표(Table) 형태의 HTML로 요약 생성
    def get_config_summary(self, cfg_path):
        params = {}
        try:
            with open(cfg_path, 'r') as f:
                for line in f:
                    # 인라인 주석 제거 및 공백 정리
                    clean_line = line.split('#')[0].strip()
                    if not clean_line: continue
                    parts = clean_line.split()
                    if len(parts) >= 2:
                        key = parts[0]
                        val = " ".join(parts[1:])
                        params[key] = val
        except Exception:
            return "Config file read error."

        if not params: return "No valid parameters found."

        # HTML Table 구조로 직관적인 확장 바 구현
        html = f"""
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
                <td colspan='2' style='padding:2px; color:#D32F2F;'><b>THR:</b> {params.get('THR', '-')}</td>
            </tr>
        </table>
        """
        return html

    def start_manual(self, subrun_idx=1):
        self.auto_mode = "MANUAL"; self.current_subrun = subrun_idx
        self.max_subruns = self.sp_sub_max.value()
        self.start_time = datetime.now()
        self.last_stats = {'events': 0, 'size': 0.0, 'rate': 0.0} 
        
        run_str = f"{self.input_run.text()}_{self.current_subrun:03d}" if self.max_subruns > 1 else self.input_run.text()
        self.sig_mode.emit(f"RUN [{run_str}]")
        
        # 💡 요약 시그널 전송 (HTML 포맷)
        cfg_summary = self.get_config_summary(self.combo_cfg.currentData())
        self.sig_config.emit(cfg_summary)
        
        out_file = os.path.join(self.data_dir, f"run_{run_str}.dat")
        args = ["-f", self.combo_cfg.currentData(), "-o", out_file]
        if self.rb_evt.isChecked(): args.extend(["-n", str(self.spin_val.value())])
        elif self.rb_time.isChecked(): args.extend(["-t", str(self.spin_val.value())])
        
        bin_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../bin/frontend_500_mini"))
        self.daq_manager.start_process(bin_path, args)

    def start_scan(self):
        self.auto_mode = "SCAN"; self.start_time = datetime.now()
        self.scan_queue = list(range(self.sp_start.value(), self.sp_end.value() + 1, self.sp_step.value()))
        self.run_scan_step()

    def run_scan_step(self):
        if not self.scan_queue: 
            self.auto_mode = "NONE"; self.sig_mode.emit("IDLE")
            self.sig_config.emit("Ready to start...")
            self.sig_log.emit("<span style='color:#388E3C; font-weight:bold;'>[AUTO] All scan steps completed.</span>", False)
            return
        
        curr_thr = self.scan_queue.pop(0)
        self.start_time = datetime.now()
        self.last_stats = {'events': 0, 'size': 0.0, 'rate': 0.0}
        self.sig_mode.emit(f"SCAN [THR={curr_thr}]")
        
        # 💡 스캔용 요약 시그널 전송 (HTML 포맷)
        cfg_summary = self.get_config_summary(self.combo_cfg.currentData())
        scan_html = f"<div style='color:#D32F2F; font-weight:bold; margin-bottom:5px;'>Scan Target THR: {curr_thr}</div>{cfg_summary}"
        self.sig_config.emit(scan_html)
        
        out_file = os.path.join(self.data_dir, f"run_scan_thr{curr_thr}.dat")
        bin_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../bin/frontend_500_mini"))
        self.daq_manager.start_process(bin_path, ["-f", self.combo_cfg.currentData(), "-o", out_file, "-t", str(self.sp_scan_time.value())])

    def handle_process_state(self, is_running):
        self.sub_tabs.setEnabled(not is_running)
        self.input_run.setEnabled(not is_running)
        
        if not is_running and self.start_time is not None:
            end_time = datetime.now()
            elapsed = (end_time - self.start_time).total_seconds()
            
            quality_text = self.combo_qual.currentText().split(" ")[1] 
            self.db.insert_frontend_summary(
                run_num=int(self.input_run.text()) if self.input_run.text().isdigit() else 0,
                subrun=self.current_subrun if self.auto_mode == "MANUAL" else 0,
                start=self.start_time.strftime("%Y-%m-%d %H:%M:%S"),
                end=end_time.strftime("%Y-%m-%d %H:%M:%S"),
                elapsed=round(elapsed, 2),
                events=self.last_stats.get('events', 0),
                size_mb=float(self.last_stats.get('size', 0.0)),
                rate=float(self.last_stats.get('rate', 0.0)),
                mode=self.auto_mode,
                quality=quality_text,
                comments=self.input_cmt.text()
            )
            self.sig_log.emit("<span style='color:#388E3C; font-weight:bold;'>[DB] Summary safely stored to Database.</span>", False)
            self.start_time = None 

            if self.auto_mode == "MANUAL" and self.current_subrun < self.max_subruns:
                idle = self.sp_idle.value()
                self.sig_mode.emit(f"IDLE ({idle}s)")
                self.sig_log.emit(f"<span style='color:#F57C00; font-weight:bold;'>[AUTO] Waiting {idle} seconds for next Sub-run...</span>", False)
                QTimer.singleShot(idle * 1000, lambda: self.start_manual(self.current_subrun + 1))
            elif self.auto_mode == "SCAN":
                idle = self.sp_scan_idle.value()
                self.sig_mode.emit(f"IDLE ({idle}s)")
                QTimer.singleShot(idle * 1000, self.run_scan_step)
            else:
                self.auto_mode = "NONE"; self.sig_mode.emit("IDLE")
                self.sig_config.emit("Ready to start...")

    def force_abort(self):
        self.auto_mode = "NONE"; self.scan_queue.clear(); self.max_subruns = 1
        self.daq_manager.stop_process()
        self.sig_config.emit("Ready to start...")
        if self.mon_process.state() == QProcess.Running: self.mon_process.terminate()

    def start_monitor(self):
        if self.mon_process.state() != QProcess.Running:
            mon_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../bin/online_monitor"))
            run_str = f"{self.input_run.text()}_{self.current_subrun:03d}" if self.max_subruns > 1 else self.input_run.text()
            self.mon_process.start(mon_path, [os.path.join(self.data_dir, f"run_{run_str}.dat")])
            self.sig_log.emit("<span style='color:#0288D1; font-weight:bold;'>[MONITOR] Live Viewer Started.</span>", False)

    def stop_monitor(self):
        if self.mon_process.state() == QProcess.Running: 
            self.mon_process.terminate()
            self.sig_log.emit("<span style='color:#8E24AA; font-weight:bold;'>[MONITOR] Live Viewer Stopped.</span>", False)
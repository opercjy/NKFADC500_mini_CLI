import os
import glob
from datetime import datetime
from core.DatabaseManager import DatabaseManager
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
                             QLabel, QGroupBox, QLineEdit, QComboBox, QSpinBox, 
                             QGridLayout)
from PyQt5.QtCore import Qt, pyqtSignal, QTimer, QProcess
from core.ProcessManager import ProcessManager
from widgets.path_controller import PathControllerWidget  

class DaqTab(QWidget):
    sig_log = pyqtSignal(str, bool)
    sig_stat = pyqtSignal(dict)
    sig_mode = pyqtSignal(str)
    sig_config = pyqtSignal(str) 

    def __init__(self, data_dir, config_dir):
        super().__init__()
        self.data_dir = data_dir
        self.config_dir = config_dir
        self.start_time = None
        self.auto_mode = "NONE"
        self.current_out_file = ""
        
        self.db = DatabaseManager()
        self.last_stats = {'events': 0, 'size': 0.0, 'rate': 0.0}
        
        self.daq_manager = ProcessManager()
        self.mon_process = QProcess()
        
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
        path_layout.addWidget(self.path_data); path_layout.addWidget(self.path_config)
        layout.addLayout(path_layout)

        cfg_group = QGroupBox("Run & Config Settings")
        cfg_layout = QGridLayout()
        
        # 💡 [마이너 패치] 파일 접두사(Prefix)와 Run Number를 분리하여 나란히 배치
        cfg_layout.addWidget(QLabel("File Name (Prefix_Run):"), 0, 0)
        prefix_run_lay = QHBoxLayout()
        prefix_run_lay.setContentsMargins(0, 0, 0, 0)
        
        self.input_prefix = QLineEdit()
        self.input_prefix.setText("run") # 기본값 유지
        self.input_prefix.setToolTip("Custom prefix (e.g., laser, calib, bg)")
        
        self.input_run = QLineEdit()
        
        prefix_run_lay.addWidget(self.input_prefix)
        prefix_run_lay.addWidget(QLabel("_"))
        prefix_run_lay.addWidget(self.input_run)
        
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

        cfg_group.setLayout(cfg_layout); layout.addWidget(cfg_group)

        control_layout = QHBoxLayout()
        
        std_group = QGroupBox("🕹️ Manual / Long-Term DAQ")
        std_group.setStyleSheet("QGroupBox { border: 2px solid #4CAF50; border-radius: 6px; background-color: #F1F8E9;} QGroupBox::title { color: #2E7D32; }")
        std_lay = QVBoxLayout()
        
        mode_lay = QHBoxLayout()
        mode_lay.addWidget(QLabel("Run Mode:"))
        self.combo_run_mode = QComboBox()
        self.combo_run_mode.addItems(["Continuous (무한 수집)", "Target Events (이벤트 종료)", "Target Time (시간 종료)", "Long-Term (다중 분할 런)"])
        self.combo_run_mode.currentIndexChanged.connect(self.update_std_ui)
        mode_lay.addWidget(self.combo_run_mode)
        std_lay.addLayout(mode_lay)
        
        self.widget_val = QWidget()
        val_lay = QHBoxLayout(self.widget_val); val_lay.setContentsMargins(0,0,0,0)
        self.lbl_target = QLabel("Target Limit:")
        self.spin_target = QSpinBox(); self.spin_target.setRange(1, 100000000); self.spin_target.setValue(10000)
        val_lay.addWidget(self.lbl_target); val_lay.addWidget(self.spin_target)
        std_lay.addWidget(self.widget_val)
        
        self.widget_sub = QWidget()
        sub_lay = QHBoxLayout(self.widget_sub); sub_lay.setContentsMargins(0,0,0,0)
        self.spin_sub_max = QSpinBox(); self.spin_sub_max.setRange(1, 9999); self.spin_sub_max.setValue(5)
        self.spin_idle = QSpinBox(); self.spin_idle.setRange(0, 3600); self.spin_idle.setValue(5)
        sub_lay.addWidget(QLabel("Total Chunks:")); sub_lay.addWidget(self.spin_sub_max)
        sub_lay.addWidget(QLabel("Idle(s):")); sub_lay.addWidget(self.spin_idle)
        std_lay.addWidget(self.widget_sub)
        std_lay.addStretch()
        
        self.btn_start_man = QPushButton("▶ START MANUAL DAQ")
        self.btn_start_man.setStyleSheet("background-color: #4CAF50; color: white; padding: 12px; font-weight:bold; font-size:14px;")
        self.btn_stop_man = QPushButton("⏹ STOP MANUAL DAQ")
        self.btn_stop_man.setStyleSheet("background-color: #F44336; color: white; padding: 12px; font-weight:bold; font-size:14px;")
        self.btn_stop_man.setEnabled(False)
        std_lay.addWidget(self.btn_start_man); std_lay.addWidget(self.btn_stop_man)
        std_group.setLayout(std_lay)
        control_layout.addWidget(std_group)
        
        scan_group = QGroupBox("🔄 Threshold (ADC) Auto Scan")
        scan_group.setStyleSheet("QGroupBox { border: 2px solid #FF9800; border-radius: 6px; background-color: #FFF3E0;} QGroupBox::title { color: #E65100; }")
        scan_lay = QVBoxLayout()
        
        scan_grid = QGridLayout()
        self.sp_start = QSpinBox(); self.sp_start.setRange(1, 4000); self.sp_start.setValue(20)
        self.sp_end = QSpinBox(); self.sp_end.setRange(1, 4000); self.sp_end.setValue(100)
        self.sp_step = QSpinBox(); self.sp_step.setRange(1, 500); self.sp_step.setValue(10)
        scan_grid.addWidget(QLabel("Start ADC:"), 0, 0); scan_grid.addWidget(self.sp_start, 0, 1)
        scan_grid.addWidget(QLabel("End ADC:"), 0, 2); scan_grid.addWidget(self.sp_end, 0, 3)
        scan_grid.addWidget(QLabel("Step ADC:"), 1, 0); scan_grid.addWidget(self.sp_step, 1, 1)
        scan_lay.addLayout(scan_grid)
        
        scan_opt = QHBoxLayout()
        self.combo_scan_mode = QComboBox()
        self.combo_scan_mode.addItems(["Time (sec)", "Events"])
        self.spin_scan_val = QSpinBox(); self.spin_scan_val.setRange(1, 10000000); self.spin_scan_val.setValue(10)
        self.spin_scan_idle = QSpinBox(); self.spin_scan_idle.setRange(0, 3600); self.spin_scan_idle.setValue(3)
        scan_opt.addWidget(self.combo_scan_mode); scan_opt.addWidget(self.spin_scan_val)
        scan_opt.addWidget(QLabel("Idle(s):")); scan_opt.addWidget(self.spin_scan_idle)
        scan_lay.addLayout(scan_opt)
        
        scan_lay.addStretch()
        
        self.btn_scan = QPushButton("🔄 START THRESHOLD SCAN")
        self.btn_scan.setStyleSheet("background-color: #FF9800; color: white; padding: 12px; font-weight:bold; font-size:14px;")
        scan_lay.addWidget(self.btn_scan)
        scan_group.setLayout(scan_lay)
        control_layout.addWidget(scan_group)

        layout.addLayout(control_layout)
        self.update_std_ui() 
        layout.addStretch()

    def update_std_ui(self):
        idx = self.combo_run_mode.currentIndex()
        if idx == 0: 
            self.widget_val.setVisible(False); self.widget_sub.setVisible(False)
        elif idx == 1: 
            self.widget_val.setVisible(True); self.widget_sub.setVisible(False)
            self.lbl_target.setText("Target Events:")
        elif idx == 2: 
            self.widget_val.setVisible(True); self.widget_sub.setVisible(False)
            self.lbl_target.setText("Target Time (sec):")
        elif idx == 3: 
            self.widget_val.setVisible(True); self.widget_sub.setVisible(True)
            self.lbl_target.setText("Per Chunk Limit (sec/evt):")

    def refresh_config_list(self):
        self.combo_cfg.clear()
        current_cfg_dir = self.path_config.get_path()
        
        if not os.path.exists(current_cfg_dir):
            self.sig_log.emit(f"\033[1;31m[ERROR] Config 경로를 찾을 수 없습니다: {current_cfg_dir}\033[0m", True)
            return
            
        cfg_files = []
        for f in os.listdir(current_cfg_dir):
            if f.lower().endswith('.config') or f.lower().endswith('.cfg'):
                cfg_files.append(f)
        
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

    def generate_out_filename(self, subrun=0):
        # 💡 [마이너 패치] 사용자가 입력한 Prefix와 Run Number를 조합하여 안전하게 파일명 생성
        prefix = self.input_prefix.text().strip()
        if not prefix: prefix = "run"
        
        run_num = self.input_run.text().strip()
        if not run_num: run_num = "999"
        
        target_dir = self.path_data.get_path()
        
        if self.auto_mode == "MANUAL" and self.max_subruns > 1:
            fname = f"{prefix}_{run_num}_{subrun:03d}.dat"
        else:
            fname = f"{prefix}_{run_num}.dat"
            
        return os.path.join(target_dir, fname)

    def get_config_summary(self, cfg_path):
        params = {}
        try:
            with open(cfg_path, 'r') as f:
                for line in f:
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
        cfg_file = self.combo_cfg.currentData()
        if not cfg_file:
            self.sig_log.emit("\033[1;31m[ERROR] 설정 파일을 찾을 수 없습니다!\033[0m", True)
            return

        self.auto_mode = "MANUAL"
        idx = self.combo_run_mode.currentIndex()
        if subrun_idx == 1:
            self.max_subruns = self.spin_sub_max.value() if idx == 3 else 1
            
        self.current_subrun = subrun_idx
        self.current_out_file = self.generate_out_filename(subrun_idx)
        
        script_dir = os.path.dirname(os.path.abspath(__file__))
        bin_path = os.path.abspath(os.path.join(script_dir, "../../../bin/frontend_500_mini"))
        
        args = ["-f", cfg_file, "-o", self.current_out_file]
        
        if idx == 1: args.extend(["-n", str(self.spin_target.value())]) 
        elif idx in [2, 3]: args.extend(["-t", str(self.spin_target.value())]) 
            
        self.btn_start_man.setEnabled(False); self.btn_scan.setEnabled(False)
        self.btn_stop_man.setEnabled(True)
        self.start_time = datetime.now()
        
        # UI 모드 라벨 렌더링에 사용할 이름 조합
        prefix = self.input_prefix.text().strip() or "run"
        run_str = f"{prefix}_{self.input_run.text()}_{self.current_subrun:03d}" if self.max_subruns > 1 else f"{prefix}_{self.input_run.text()}"
        self.sig_mode.emit(f"RUN [{run_str}]")
        
        cfg_summary = self.get_config_summary(cfg_file)
        self.sig_config.emit(cfg_summary)

        self.sig_log.emit("\033[1;36m[SYSTEM] Starting DAQ...\033[0m", False)

        self.daq_manager.start_process(bin_path, args)

        if self.mon_process.state() == QProcess.Running:
            self.stop_monitor()
            self.mon_process.waitForFinished(500)
            self.start_monitor()

    def start_scan(self):
        cfg_file = self.combo_cfg.currentData()
        if not cfg_file:
            self.sig_log.emit("\033[1;31m[ERROR] 설정 파일을 찾을 수 없습니다!\033[0m", True)
            return

        self.auto_mode = "SCAN"; self.start_time = datetime.now()
        self.scan_queue = list(range(self.sp_start.value(), self.sp_end.value() + 1, self.sp_step.value()))
        self.btn_start_man.setEnabled(False); self.btn_scan.setEnabled(False)
        self.btn_stop_man.setEnabled(True)
        self.run_scan_step()

    def run_scan_step(self):
        if not self.scan_queue: 
            self.auto_mode = "NONE"; self.sig_mode.emit("IDLE")
            self.btn_start_man.setEnabled(True); self.btn_scan.setEnabled(True)
            self.btn_stop_man.setEnabled(False)
            self.sig_log.emit("\033[1;32m[AUTO] All scan steps completed.\033[0m", False)
            
            if self.input_run.text().isdigit():
                next_run = str(int(self.input_run.text()) + 1)
                self.input_run.setText(next_run)
            return
        
        curr_thr = self.scan_queue.pop(0)
        self.start_time = datetime.now()
        self.last_stats = {'events': 0, 'size': 0.0, 'rate': 0.0}
        self.sig_mode.emit(f"SCAN [THR={curr_thr}]")
        
        cfg_summary = self.get_config_summary(self.combo_cfg.currentData())
        scan_html = f"<div style='color:#D32F2F; font-weight:bold; margin-bottom:5px;'>Scan Target THR: {curr_thr}</div>{cfg_summary}"
        self.sig_config.emit(scan_html)
        
        prefix = self.input_prefix.text().strip() or "run"
        self.current_out_file = os.path.join(self.path_data.get_path(), f"{prefix}_{self.input_run.text()}_thr{curr_thr}.dat") 
        
        script_dir = os.path.dirname(os.path.abspath(__file__))
        bin_path = os.path.abspath(os.path.join(script_dir, "../../../bin/frontend_500_mini"))
        
        args = ["-f", self.combo_cfg.currentData(), "-o", self.current_out_file]
        if self.combo_scan_mode.currentIndex() == 0: args.extend(["-t", str(self.spin_scan_val.value())]) 
        else: args.extend(["-n", str(self.spin_scan_val.value())]) 
        
        self.sig_log.emit(f"\033[1;36m[SYSTEM] Starting Scan Step (THR={curr_thr})...\033[0m", False)
        
        self.daq_manager.start_process(bin_path, args)

        if self.mon_process.state() == QProcess.Running:
            self.stop_monitor()
            self.mon_process.waitForFinished(500)
            self.start_monitor()

    def handle_process_state(self, is_running):
        self.combo_cfg.setEnabled(not is_running)
        self.input_run.setEnabled(not is_running)
        self.input_prefix.setEnabled(not is_running) # 💡 실행 중엔 접두사도 변경 금지
        
        if not is_running and self.start_time is not None:
            end_time = datetime.now()
            elapsed = (end_time - self.start_time).total_seconds()
            
            quality_text = self.combo_qual.currentText().split(" ")[1] 
            cfg_file = self.combo_cfg.currentData()
            cfg_name = os.path.basename(cfg_file) if cfg_file else "Unknown"
            
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
                comments=self.input_cmt.text(),
                config_file=cfg_name
            )
            self.sig_log.emit("\033[1;32m[DB] Summary safely stored to Database.\033[0m", False)
            self.start_time = None 

            if self.auto_mode == "MANUAL" and self.current_subrun < self.max_subruns:
                idle = self.spin_idle.value()
                self.sig_mode.emit(f"IDLE ({idle}s)")
                QTimer.singleShot(idle * 1000, lambda: self.start_manual(self.current_subrun + 1))
            elif self.auto_mode == "SCAN":
                idle = self.spin_scan_idle.value()
                self.sig_mode.emit(f"IDLE ({idle}s)")
                QTimer.singleShot(idle * 1000, self.run_scan_step)
            else:
                self.auto_mode = "NONE"
                self.sig_mode.emit("IDLE")
                
                # 💡 [핵심] 기존 런 넘버 자동 증가 로직은 완벽히 보존됨
                if self.input_run.text().isdigit():
                    next_run = str(int(self.input_run.text()) + 1)
                    self.input_run.setText(next_run)
                
                self.btn_start_man.setEnabled(True); self.btn_scan.setEnabled(True)
                self.btn_stop_man.setEnabled(False)

    def update_stats(self, stat_dict):
        self.last_stats.update(stat_dict)
        self.sig_stat.emit(stat_dict)
        if 'log' in stat_dict:
            self.sig_log.emit(stat_dict['log'], stat_dict.get('is_error', False))

    def stop_daq(self):
        self.sig_log.emit("\033[1;33mSending Stop signal...\033[0m", False)
        self.daq_manager.stop_process()
        self.btn_start_man.setEnabled(True); self.btn_scan.setEnabled(True)
        self.btn_stop_man.setEnabled(False)

    def force_abort(self):
        self.auto_mode = "NONE"
        self.daq_manager.stop_process()
        self.btn_start_man.setEnabled(True); self.btn_scan.setEnabled(True)
        self.btn_stop_man.setEnabled(False)

    def start_monitor(self):
        if self.mon_process.state() == QProcess.Running: return
        self.sig_log.emit("\033[1;36mStarting DQM Monitor...\033[0m", False)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        bin_path = os.path.abspath(os.path.join(script_dir, "../../../bin/online_monitor"))
        
        if not self.current_out_file:
            prefix = self.input_prefix.text().strip() or "run"
            run_str = f"{self.input_run.text()}_{self.current_subrun:03d}" if self.max_subruns > 1 else self.input_run.text()
            self.current_out_file = os.path.join(self.data_dir, f"{prefix}_{run_str}.dat")
            
        self.mon_process.start(bin_path, [self.current_out_file])
        
    def stop_monitor(self):
        if self.mon_process.state() == QProcess.Running:
            self.mon_process.kill()
            self.mon_process.waitForFinished()
            self.sig_log.emit("\033[1;35mDQM Monitor stopped.\033[0m", False)
            
    def clear_monitor(self):
        if self.mon_process.state() == QProcess.Running:
            self.mon_process.write(b"c\n")
            self.sig_log.emit("\033[1;36m[MONITOR] Histograms cleared by user command.\033[0m", False)

    def is_running(self):
        return self.start_time is not None
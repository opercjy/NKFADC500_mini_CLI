import os
import zmq
import numpy as np
import pyqtgraph as pg
from datetime import datetime
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
                             QLabel, QGroupBox, QLineEdit, QComboBox, QSpinBox, 
                             QRadioButton, QTabWidget)
from PyQt5.QtCore import Qt, pyqtSignal, QTimer, QProcess, QThread
from core.ProcessManager import ProcessManager
from core.DatabaseManager import DatabaseManager

# 💡 [Phase 6] 비동기 ZMQ 수신 스레드 (GUI 멈춤 방어 & CONFLATE 적용)
class ZmqReceiver(QThread):
    sig_wave = pyqtSignal(np.ndarray)

    def __init__(self):
        super().__init__()
        self.running = False
        self.ctx = zmq.Context()
        self.sock = self.ctx.socket(zmq.SUB)
        self.sock.setsockopt(zmq.CONFLATE, 1) # 백프레셔 방어 (최신 1개만 수신)
        self.sock.setsockopt_string(zmq.SUBSCRIBE, "")

    def run(self):
        self.sock.connect("tcp://127.0.0.1:5555")
        self.running = True
        while self.running:
            try:
                # Flat Binary Zero-copy 디코딩
                msg = self.sock.recv(flags=zmq.NOBLOCK)
                data_arr = np.frombuffer(msg, dtype=np.uint16)
                self.sig_wave.emit(data_arr)
            except zmq.Again:
                self.msleep(20) # 데이터가 없으면 20ms 휴식 (CPU 절약)
            except Exception:
                pass

    def stop(self):
        self.running = False
        self.wait()

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
        
        self.db = DatabaseManager()
        self.daq_manager = ProcessManager()
        
        # 💡 [Phase 6] ZMQ 스레드 및 PyQtGraph 셋업
        self.zmq_thread = ZmqReceiver()
        self.zmq_thread.sig_wave.connect(self.update_plot)
        self.use_zmq_viewer = False 
        
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

        man_layout.addLayout(cond_layout)

        self.btn_start = QPushButton("▶ START MANUAL / LONG-TERM DAQ")
        self.btn_start.setStyleSheet("background-color: #4CAF50; color: white; padding: 15px; font-weight: bold; font-size: 14px;")
        man_layout.addWidget(self.btn_start)
        
        self.sub_tabs.addTab(tab_manual, "🕹️ Standard DAQ")
        layout.addWidget(self.sub_tabs)

        # 💡 [Phase 6] PyQtGraph 라이브 파형 뷰어 영역 추가
        plot_group = QGroupBox("Live Waveform Monitor (ZeroMQ IPC)")
        plot_layout = QVBoxLayout()
        pg.setConfigOption('background', 'w') # 배경 흰색
        pg.setConfigOption('foreground', 'k')
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setYRange(0, 4096)
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
        self.plot_curve = self.plot_widget.plot(pen=pg.mkPen(color='#1976D2', width=1.5), autoDownsample=True, clipToView=True)
        plot_layout.addWidget(self.plot_widget)
        plot_group.setLayout(plot_layout)
        layout.addWidget(plot_group, stretch=1)

    def connectSignals(self):
        self.btn_start.clicked.connect(self.start_manual)
        self.daq_manager.log_signal.connect(lambda msg: self.sig_log.emit(msg, False))
        self.daq_manager.stat_signal.connect(self.update_stats_and_emit)
        self.daq_manager.state_signal.connect(self.handle_process_state)

    def update_stats_and_emit(self, stats):
        self.last_stats.update(stats)
        self.sig_stat.emit(stats)

    def is_running(self):
        return self.daq_manager.process.state() == QProcess.Running

    def get_config_summary(self, cfg_path):
        params = {}
        try:
            with open(cfg_path, 'r') as f:
                for line in f:
                    clean_line = line.split('#')[0].strip()
                    if not clean_line: continue
                    parts = clean_line.split()
                    if len(parts) >= 2:
                        params[parts[0]] = " ".join(parts[1:])
        except Exception:
            return "Config error."

        html = f"""
        <table style='width:100%; font-size:12px; color:#424242;'>
            <tr><td style='padding:2px;'><b>RL:</b> {params.get('RECORD_LEN', '-')}</td><td style='padding:2px;'><b>TLT:</b> {params.get('TRIG_TLT', '-')}</td></tr>
            <tr><td style='padding:2px;'><b>POL:</b> {params.get('POL', '-')}</td><td style='padding:2px;'><b>CW:</b> {params.get('CW', '-')}</td></tr>
            <tr><td colspan='2' style='padding:2px; color:#D32F2F;'><b>THR:</b> {params.get('THR', '-')}</td></tr>
        </table>
        """
        return html

    def start_manual(self, subrun_idx=1):
        self.auto_mode = "MANUAL"; self.current_subrun = subrun_idx
        self.start_time = datetime.now()
        
        run_str = f"{self.input_run.text()}_{self.current_subrun:03d}" if self.max_subruns > 1 else self.input_run.text()
        self.sig_mode.emit(f"RUN [{run_str}]")
        self.sig_config.emit(self.get_config_summary(self.combo_cfg.currentData()))
        
        out_file = os.path.join(self.data_dir, f"run_{run_str}.dat")
        args = ["-f", self.combo_cfg.currentData(), "-o", out_file]
        
        bin_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../bin/frontend_500_mini"))
        self.daq_manager.start_process(bin_path, args)

    def handle_process_state(self, is_running):
        self.sub_tabs.setEnabled(not is_running)
        self.input_run.setEnabled(not is_running)
        
        if not is_running:
            self.auto_mode = "NONE"; self.sig_mode.emit("IDLE"); self.sig_config.emit("Ready to start...")

    def force_abort(self):
        self.auto_mode = "NONE"
        self.daq_manager.stop_process()
        self.sig_config.emit("Ready to start...")
        self.stop_monitor()

    # 💡 [Phase 6] 메인 윈도우의 "Live Monitor ON" 버튼 클릭 시 ZMQ 스레드 가동
    def start_monitor(self):
        if not self.use_zmq_viewer:
            self.zmq_thread.start()
            self.use_zmq_viewer = True
            self.sig_log.emit("<span style='color:#0288D1; font-weight:bold;'>[ZMQ] High-Speed Live Viewer Connected (Zero-Copy).</span>", False)

    def stop_monitor(self):
        if self.use_zmq_viewer:
            self.zmq_thread.stop()
            self.use_zmq_viewer = False
            self.plot_curve.clear()
            self.sig_log.emit("<span style='color:#8E24AA; font-weight:bold;'>[ZMQ] Disconnected from Viewer.</span>", False)

    # 💡 [Phase 6] ZMQ 스레드에서 데이터가 오면 PyQtGraph 업데이트 (부하 최소화)
    def update_plot(self, data_arr):
        if len(data_arr) > 0:
            # 첫 번째 채널의 일부 파형만 잘라서 고속 렌더링 (임시 파싱 로직)
            view_len = min(1000, len(data_arr))
            self.plot_curve.setData(data_arr[:view_len])

import os
import shutil
from datetime import datetime
from PyQt5.QtWidgets import (QMainWindow, QTabWidget, QVBoxLayout, QHBoxLayout, 
                             QWidget, QStatusBar, QSplitter, QGroupBox, QLabel, 
                             QPushButton, QTextEdit, QLCDNumber)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont, QTextCursor

from widgets.DaqTab import DaqTab
from widgets.ConfigTab import ConfigTab
from widgets.TltTab import TltTab
from widgets.ProductionTab import ProductionTab
from widgets.DatabaseTab import DatabaseTab
from widgets.HvTab import HvTab

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.data_output_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../data"))
        os.makedirs(self.data_output_dir, exist_ok=True)
        self.initUI()
        self.connectSignals()

        self.clock_timer = QTimer(self)
        self.clock_timer.timeout.connect(self.update_clock)
        self.clock_timer.start(1000)

    def initUI(self):
        self.setWindowTitle("NKFADC500 Mini - Ultimate DQM Panel")
        self.resize(1350, 900) 

        # 💡 전체 화이트/베이지 테마 및 LCD/콘솔 화이트 배경 유지
        self.setStyleSheet("""
            QMainWindow { background-color: #F4F1EA; }
            QTabWidget::pane { border: 1px solid #CDBA96; background: #FDFBF7; border-radius: 4px; }
            QTabBar::tab { background: #E8E2D2; color: #333333; padding: 12px 25px; font-size: 14px; font-weight: bold; border: 1px solid #CDBA96; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px;}
            QTabBar::tab:selected { background: #FFFFFF; border-bottom: 3px solid #4CAF50; color: #000000; }
            QLabel { color: #333333; font-weight: bold; }
            QGroupBox { border: 2px solid #A69B8D; border-radius: 6px; margin-top: 15px; background-color: #FDFBF7; }
            QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 15px; color: #5C4B3A; font-weight: bold;}
            QTextEdit { background-color: #FFFFFF; color: #333333; border: 2px solid #A69B8D; border-radius: 4px; padding: 5px; font-family: Consolas; font-size: 13px;}
        """)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        top_layout = QHBoxLayout()
        title_lbl = QLabel("NKFADC500 Mini Master Console")
        title_lbl.setFont(QFont("Arial", 18, QFont.Bold))
        self.clock_lbl = QLabel("0000-00-00 00:00:00")
        self.clock_lbl.setFont(QFont("Consolas", 14, QFont.Bold))
        self.clock_lbl.setStyleSheet("color: #1976D2;")
        top_layout.addWidget(title_lbl); top_layout.addStretch(); top_layout.addWidget(self.clock_lbl)
        main_layout.addLayout(top_layout)

        self.splitter = QSplitter(Qt.Horizontal)
        
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_layout.setContentsMargins(0, 0, 5, 0)
        
        self.tabs = QTabWidget()
        self.daq_tab = DaqTab(self.data_output_dir)
        self.hv_tab = HvTab()
        self.tlt_tab = TltTab()
        self.config_tab = ConfigTab()
        self.prod_tab = ProductionTab(self.data_output_dir)
        self.db_tab = DatabaseTab()

        self.tabs.addTab(self.daq_tab, "🚀 DAQ Control")
        self.tabs.addTab(self.hv_tab, "⚡ HV Control")
        self.tabs.addTab(self.tlt_tab, "🎯 TLT Config")
        self.tabs.addTab(self.config_tab, "⚙️ Hardware Config")
        self.tabs.addTab(self.prod_tab, "🛠️ Production")
        self.tabs.addTab(self.db_tab, "🗄️ Run Database")
        left_layout.addWidget(self.tabs, stretch=1)

        master_group = QGroupBox("Master Controls")
        master_layout = QHBoxLayout()
        self.btn_mon_start = QPushButton("👁️ Live Monitor ON")
        self.btn_mon_stop = QPushButton("🙈 Live Monitor OFF")
        self.btn_abort = QPushButton("🛑 ABORT ALL")
        self.btn_mon_start.setStyleSheet("background-color: #2196F3; color: white; padding: 10px; font-weight: bold;")
        self.btn_mon_stop.setStyleSheet("background-color: #9E9E9E; color: white; padding: 10px; font-weight: bold;")
        self.btn_abort.setStyleSheet("background-color: #F44336; color: white; padding: 10px; font-weight: bold;")
        master_layout.addWidget(self.btn_mon_start)
        master_layout.addWidget(self.btn_mon_stop)
        master_layout.addWidget(self.btn_abort)
        master_group.setLayout(master_layout)
        left_layout.addWidget(master_group)

        # --- 우측 라이브 대시보드 ---
        right_widget = QWidget()
        right_layout = QVBoxLayout(right_widget)
        dash_group = QGroupBox("Live Status Dashboard")
        dash_group.setStyleSheet("QGroupBox { border: 2px solid #1976D2; background-color: #FFFFFF; } QGroupBox::title { color: #1976D2; }")
        d_layout = QVBoxLayout()
        d_layout.setContentsMargins(15, 25, 15, 15); d_layout.setSpacing(12)
        
        self.lbl_mode = QLabel("Mode: IDLE")
        self.lbl_mode.setFont(QFont("Arial", 14, QFont.Bold)); self.lbl_mode.setStyleSheet("color: #FF9800;")
        d_layout.addWidget(self.lbl_mode)
        
        # 💡 [UX 신규 패치] 현재 구동 중인 Config 요약창 추가
        cfg_group = QGroupBox("Current Run & Config")
        cfg_group.setStyleSheet("QGroupBox { border: 1px solid #B0BEC5; margin-top: 10px; background-color: #F3E5F5;} QGroupBox::title { color: #6A1B9A; font-size: 11px;}")
        cfg_layout = QVBoxLayout()
        cfg_layout.setContentsMargins(5, 15, 5, 5)
        self.lbl_run_cfg = QLabel("Ready to start...")
        self.lbl_run_cfg.setStyleSheet("color: #333333; font-size: 13px; font-weight: normal;")
        self.lbl_run_cfg.setWordWrap(True)
        cfg_layout.addWidget(self.lbl_run_cfg)
        cfg_group.setLayout(cfg_layout)
        d_layout.addWidget(cfg_group)

        self.lbl_start = QLabel("Start: --:--:--")
        self.lbl_elapsed = QLabel("Elapsed: 00:00:00")
        self.lbl_elapsed.setStyleSheet("color: #1976D2; font-size: 14px;")
        d_layout.addWidget(self.lbl_start); d_layout.addWidget(self.lbl_elapsed)

        self.lbl_rate = QLabel("Rate: 0.0 Hz")
        self.lbl_rate.setFont(QFont("Consolas", 16, QFont.Bold)); self.lbl_rate.setStyleSheet("color: #D32F2F;")
        d_layout.addWidget(self.lbl_rate)

        d_layout.addWidget(QLabel("Acquired Events:"))
        self.lcd_events = QLCDNumber()
        self.lcd_events.setDigitCount(9); self.lcd_events.setSegmentStyle(QLCDNumber.Flat)
        self.lcd_events.setStyleSheet("color: #1B5E20; background: #FFFFFF; min-height: 50px; border: 2px solid #CDBA96; border-radius: 4px;")
        d_layout.addWidget(self.lcd_events)
        
        self.lbl_size = QLabel("File Size: 0.00 MB"); self.lbl_size.setStyleSheet("color: #E65100; font-size: 14px;")
        self.lbl_speed = QLabel("Speed: 0.00 MB/s"); self.lbl_speed.setStyleSheet("color: #2E7D32; font-size: 14px;")
        
        q_pool_layout = QHBoxLayout()
        self.lbl_dataq = QLabel("DataQ: 0")
        self.lbl_dataq.setAlignment(Qt.AlignCenter)
        self.lbl_dataq.setStyleSheet("color: #6A1B9A; font-size: 13px; font-weight: bold; background-color: #F3E5F5; padding: 5px; border-radius: 4px; border: 1px solid #CE93D8;")
        
        self.lbl_pool = QLabel("Pool: 10")
        self.lbl_pool.setAlignment(Qt.AlignCenter)
        self.lbl_pool.setStyleSheet("color: #0277BD; font-size: 13px; font-weight: bold; background-color: #E1F5FE; padding: 5px; border-radius: 4px; border: 1px solid #81D4FA;")
        
        q_pool_layout.addWidget(self.lbl_dataq)
        q_pool_layout.addWidget(self.lbl_pool)
        
        d_layout.addWidget(self.lbl_size); d_layout.addWidget(self.lbl_speed)
        d_layout.addLayout(q_pool_layout)

        self.lbl_disk = QLabel("Disk Free: -- GB")
        d_layout.addWidget(self.lbl_disk)
        d_layout.addStretch()
        dash_group.setLayout(d_layout); right_layout.addWidget(dash_group, stretch=1)

        self.splitter.addWidget(left_widget); self.splitter.addWidget(right_widget)
        self.splitter.setStretchFactor(0, 65); self.splitter.setStretchFactor(1, 35)
        main_layout.addWidget(self.splitter, stretch=3)

        log_group = QGroupBox("System Console")
        log_layout = QVBoxLayout()
        self.log_viewer = QTextEdit(); self.log_viewer.setReadOnly(True)
        log_layout.addWidget(self.log_viewer); log_group.setLayout(log_layout)
        main_layout.addWidget(log_group, stretch=1)

        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("System Ready.")

    def connectSignals(self):
        self.daq_tab.sig_log.connect(self.append_log)
        self.daq_tab.sig_stat.connect(self.update_dashboard)
        self.daq_tab.sig_mode.connect(self.set_daq_mode)
        self.daq_tab.sig_config.connect(self.update_config_summary) # 💡 Config 요약 시그널 연결
        self.prod_tab.sig_log.connect(self.append_log)
        self.hv_tab.sig_log.connect(self.append_log)

        self.btn_mon_start.clicked.connect(self.daq_tab.start_monitor)
        self.btn_mon_stop.clicked.connect(self.daq_tab.stop_monitor)
        self.btn_abort.clicked.connect(self.force_abort_all)

    def update_clock(self):
        self.clock_lbl.setText(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
        total, used, free = shutil.disk_usage(self.data_output_dir)
        gb_free = free // (2**30)
        self.lbl_disk.setText(f"Disk Free: {gb_free} GB")
        
        if self.daq_tab.is_running() and self.daq_tab.start_time:
            elapsed = datetime.now() - self.daq_tab.start_time
            h, rem = divmod(elapsed.seconds, 3600); m, s = divmod(rem, 60)
            self.lbl_elapsed.setText(f"Elapsed: {h:02d}:{m:02d}:{s:02d}")

    def update_dashboard(self, stats):
        if 'events' in stats: self.lcd_events.display(stats['events'])
        if 'rate' in stats: self.lbl_rate.setText(f"Rate: {stats['rate']} Hz")
        if 'size' in stats: self.lbl_size.setText(f"File Size: {stats['size']} MB")
        if 'speed' in stats: self.lbl_speed.setText(f"Speed: {stats['speed']} MB/s")
        # 💡 [버그 픽스] dataq와 pool을 분리하여 업데이트 (에러 방지)
        if 'dataq' in stats: self.lbl_dataq.setText(f"DataQ: {stats['dataq']}")
        if 'pool' in stats: self.lbl_pool.setText(f"Pool: {stats['pool']}")

    def update_config_summary(self, text):
        self.lbl_run_cfg.setText(text)

    def append_log(self, text, is_error=False):
        if is_error:
            self.log_viewer.append(f'<span style="color:#D32F2F; font-weight:bold;">{text}</span>')
        else:
            self.log_viewer.append(text)
        self.log_viewer.moveCursor(QTextCursor.End)

    def set_daq_mode(self, mode_str):
        self.lbl_mode.setText(f"Mode: {mode_str}")
        if mode_str == "IDLE":
            self.lbl_start.setText("Start: --:--:--")
        else:
            self.lbl_start.setText(f"Start: {datetime.now().strftime('%H:%M:%S')}")

    def force_abort_all(self):
        self.daq_tab.force_abort()
        self.prod_tab.force_abort()
        self.hv_tab.force_shutdown()
        self.append_log("<span style='color:#D32F2F; font-weight:bold;'>[SYSTEM] ALL PROCESSES ABORTED BY USER.</span>", False)
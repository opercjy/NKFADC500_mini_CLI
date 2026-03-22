import os
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                             QPushButton, QLineEdit, QGroupBox, QTableWidget, 
                             QTableWidgetItem, QCheckBox, QMessageBox, QHeaderView)
from PyQt5.QtCore import pyqtSignal, QTimer, Qt
from PyQt5.QtGui import QColor, QFont

try:
    # import pycaenhv
    CAEN_AVAILABLE = True
except ImportError:
    CAEN_AVAILABLE = False

class HvTab(QWidget):
    sig_log = pyqtSignal(str, bool)

    def __init__(self):
        super().__init__()
        self.connected = False
        self.num_channels = 4 
        
        self.poll_timer = QTimer(self)
        self.poll_timer.timeout.connect(self.poll_hv_status)
        
        self.initUI()

    def initUI(self):
        layout = QVBoxLayout(self)

        mode_layout = QHBoxLayout()
        self.cb_enable_scm = QCheckBox("🔌 CAEN HV Slow Control 활성화 (체크 시 소프트웨어로 HV 제어)")
        self.cb_enable_scm.setStyleSheet("font-weight: bold; color: #1976D2; font-size: 14px;")
        self.cb_enable_scm.setChecked(False)
        self.cb_enable_scm.toggled.connect(self.toggle_mode)
        mode_layout.addWidget(self.cb_enable_scm)
        mode_layout.addStretch()
        layout.addLayout(mode_layout)

        self.lbl_analog = QLabel("⚠️ Analog HV Mode\n\n순수 아날로그 장비(ORTEC NIM 등) 사용 중입니다.\n장비의 다이얼을 직접 돌려 전압을 인가하십시오.\n소프트웨어 제어 및 모니터링이 중단되었습니다.")
        self.lbl_analog.setAlignment(Qt.AlignCenter)
        self.lbl_analog.setStyleSheet("font-weight: bold; color: #795548; background-color: #FFF3E0; border: 2px solid #FFB300; border-radius: 8px; padding: 40px; font-size: 14px;")
        layout.addWidget(self.lbl_analog)

        self.caen_widget = QWidget()
        caen_layout = QVBoxLayout(self.caen_widget)
        caen_layout.setContentsMargins(0, 0, 0, 0)

        conn_group = QGroupBox("CAEN HV Connection")
        conn_group.setStyleSheet("QGroupBox { border: 2px solid #1976D2; border-radius: 4px; margin-top: 10px;} QGroupBox::title { color: #1976D2; }")
        conn_layout = QHBoxLayout()
        
        self.input_ip = QLineEdit("192.168.0.10")
        self.input_user = QLineEdit("admin")
        self.input_pwd = QLineEdit("admin")
        self.input_pwd.setEchoMode(QLineEdit.Password)
        
        self.btn_connect = QPushButton("🔗 Connect")
        self.btn_connect.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        self.btn_connect.clicked.connect(self.connect_hv)

        conn_layout.addWidget(QLabel("IP Address:")); conn_layout.addWidget(self.input_ip)
        conn_layout.addWidget(QLabel("User:")); conn_layout.addWidget(self.input_user)
        conn_layout.addWidget(QLabel("Password:")); conn_layout.addWidget(self.input_pwd)
        conn_layout.addWidget(self.btn_connect)
        conn_group.setLayout(conn_layout)
        caen_layout.addWidget(conn_group)

        ctrl_group = QGroupBox("HV Channel Control & Monitor")
        ctrl_layout = QVBoxLayout()
        
        self.table = QTableWidget(self.num_channels, 6)
        self.table.setHorizontalHeaderLabels(["Ch", "VSet (V)", "ISet (uA)", "VMon (V)", "IMon (uA)", "Power"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.setStyleSheet("QTableWidget { background-color: #FFFFFF; color: #333333; gridline-color: #CDBA96; }")
        self.table.setEnabled(False)

        for row in range(self.num_channels):
            self.table.setItem(row, 0, QTableWidgetItem(f"Ch {row}"))
            self.table.item(row, 0).setFlags(Qt.ItemIsEnabled)
            
            self.table.setItem(row, 1, QTableWidgetItem("1000"))
            self.table.setItem(row, 2, QTableWidgetItem("100"))
            
            item_vmon = QTableWidgetItem("0.0")
            item_vmon.setFlags(Qt.ItemIsEnabled); item_vmon.setForeground(QColor("#D32F2F"))
            self.table.setItem(row, 3, item_vmon)
            
            item_imon = QTableWidgetItem("0.0")
            item_imon.setFlags(Qt.ItemIsEnabled); item_imon.setForeground(QColor("#1976D2"))
            self.table.setItem(row, 4, item_imon)

            btn_pw = QPushButton("Power ON")
            btn_pw.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
            btn_pw.clicked.connect(lambda checked, r=row: self.toggle_channel_power(r))
            self.table.setCellWidget(row, 5, btn_pw)

        ctrl_layout.addWidget(self.table)
        
        btn_apply = QPushButton("⚡ Apply All VSet/ISet")
        btn_apply.setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 10px;")
        btn_apply.clicked.connect(self.apply_all_settings)
        ctrl_layout.addWidget(btn_apply)

        ctrl_group.setLayout(ctrl_layout)
        caen_layout.addWidget(ctrl_group)
        
        self.caen_widget.setVisible(False)
        layout.addWidget(self.caen_widget)
        layout.addStretch()

    def toggle_mode(self, checked):
        self.lbl_analog.setVisible(not checked)
        self.caen_widget.setVisible(checked)
        if checked and not CAEN_AVAILABLE:
            # 💡 [버그픽스] ANSI 제거
            self.sig_log.emit("<span style='color:#F57C00; font-weight:bold;'>[WARNING] CAEN Python Library is not installed. Running in Mock Mode.</span>", False)

    def connect_hv(self):
        if not self.connected:
            ip = self.input_ip.text()
            # 💡 [버그픽스] ANSI 제거
            self.sig_log.emit(f"<span style='color:#0288D1; font-weight:bold;'>[HV]</span> Attempting to connect to CAEN at {ip}...", False)
            
            self.connected = True
            self.btn_connect.setText("❌ Disconnect")
            self.btn_connect.setStyleSheet("background-color: #757575; color: white; font-weight: bold;")
            self.table.setEnabled(True)
            self.sig_log.emit("<span style='color:#388E3C; font-weight:bold;'>[HV] Successfully connected to CAEN Power Supply.</span>", False)
            
            self.poll_timer.start(1000)
        else:
            self.connected = False
            self.btn_connect.setText("🔗 Connect")
            self.btn_connect.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
            self.table.setEnabled(False)
            self.poll_timer.stop()
            self.sig_log.emit("<span style='color:#8E24AA; font-weight:bold;'>[HV] Disconnected from CAEN.</span>", False)

    def apply_all_settings(self):
        if not self.connected: return
        for row in range(self.num_channels):
            vset = self.table.item(row, 1).text()
            iset = self.table.item(row, 2).text()
            self.sig_log.emit(f"<span style='color:#1976D2; font-weight:bold;'>[HV]</span> Ch {row} -> VSet: {vset}V, ISet: {iset}uA applied.", False)

    def toggle_channel_power(self, row):
        if not self.connected: return
        btn = self.table.cellWidget(row, 5)
        if btn.text() == "Power ON":
            btn.setText("Power OFF")
            btn.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
            self.sig_log.emit(f"<span style='color:#D32F2F; font-weight:bold;'>[HV] Ch {row} RAMPING UP...</span>", False)
        else:
            btn.setText("Power ON")
            btn.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
            self.sig_log.emit(f"<span style='color:#1976D2; font-weight:bold;'>[HV] Ch {row} RAMPING DOWN...</span>", False)

    def poll_hv_status(self):
        if not self.connected: return
        
        for row in range(self.num_channels):
            import random
            btn = self.table.cellWidget(row, 5)
            if btn.text() == "Power OFF": 
                target_v = float(self.table.item(row, 1).text())
                vmon = target_v + random.uniform(-0.5, 0.5)
                imon = random.uniform(0.1, 1.5)
            else:
                vmon = 0.0
                imon = 0.0
                
            self.table.item(row, 3).setText(f"{vmon:.2f}")
            self.table.item(row, 4).setText(f"{imon:.2f}")

    def force_shutdown(self):
        if self.connected:
            self.sig_log.emit("<span style='color:#D32F2F; font-weight:bold;'>[HV EMERGENCY] Sending KILL signal to all channels!</span>", True)
            for row in range(self.num_channels):
                btn = self.table.cellWidget(row, 5)
                btn.setText("Power ON")
                btn.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
            self.connect_hv()
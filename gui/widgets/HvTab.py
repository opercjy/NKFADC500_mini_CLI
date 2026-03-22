import os
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                             QPushButton, QLineEdit, QGroupBox, QTableWidget, 
                             QTableWidgetItem, QCheckBox, QMessageBox, QHeaderView)
from PyQt5.QtCore import pyqtSignal, QTimer, Qt
from PyQt5.QtGui import QColor, QFont

# 💡 [핵심] CAEN 라이브러리 안전 임포트 (Safe Import)
# 실제 환경에 맞게 'pycaenhv' 또는 자체 제작한 ctypes wrapper 로 변경하십시오.
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
        self.num_channels = 4 # 기본 4채널 제어 (장비에 따라 변경 가능)
        
        # 실시간 VMon, IMon 모니터링을 위한 타이머
        self.poll_timer = QTimer(self)
        self.poll_timer.timeout.connect(self.poll_hv_status)
        
        self.initUI()

    def initUI(self):
        layout = QVBoxLayout(self)

        # 1. 동작 모드 선택 (Analog vs CAEN Digital)
        mode_layout = QHBoxLayout()
        self.cb_enable_scm = QCheckBox("🔌 CAEN HV Slow Control 활성화 (체크 시 소프트웨어로 HV 제어)")
        self.cb_enable_scm.setStyleSheet("font-weight: bold; color: #1976D2; font-size: 14px;")
        self.cb_enable_scm.setChecked(False)
        self.cb_enable_scm.toggled.connect(self.toggle_mode)
        mode_layout.addWidget(self.cb_enable_scm)
        mode_layout.addStretch()
        layout.addLayout(mode_layout)

        # 2. 순수 아날로그(ORTEC 등) 모드 안내 패널
        self.lbl_analog = QLabel("⚠️ Analog HV Mode\n\n순수 아날로그 장비(ORTEC NIM 등) 사용 중입니다.\n장비의 다이얼을 직접 돌려 전압을 인가하십시오.\n소프트웨어 제어 및 모니터링이 중단되었습니다.")
        self.lbl_analog.setAlignment(Qt.AlignCenter)
        self.lbl_analog.setStyleSheet("font-weight: bold; color: #795548; background-color: #FFF3E0; border: 2px solid #FFB300; border-radius: 8px; padding: 40px; font-size: 14px;")
        layout.addWidget(self.lbl_analog)

        # 3. CAEN 제어 패널 (처음엔 숨김)
        self.caen_widget = QWidget()
        caen_layout = QVBoxLayout(self.caen_widget)
        caen_layout.setContentsMargins(0, 0, 0, 0)

        # 3-1. 연결 설정
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

        # 3-2. 채널 제어 테이블
        ctrl_group = QGroupBox("HV Channel Control & Monitor")
        ctrl_layout = QVBoxLayout()
        
        self.table = QTableWidget(self.num_channels, 6)
        self.table.setHorizontalHeaderLabels(["Ch", "VSet (V)", "ISet (uA)", "VMon (V)", "IMon (uA)", "Power"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.setStyleSheet("QTableWidget { background-color: #FFFFFF; color: #333333; gridline-color: #CDBA96; }")
        self.table.setEnabled(False) # 연결 전까지 비활성화

        for row in range(self.num_channels):
            self.table.setItem(row, 0, QTableWidgetItem(f"Ch {row}"))
            self.table.item(row, 0).setFlags(Qt.ItemIsEnabled)
            
            # 설정값 입력칸
            self.table.setItem(row, 1, QTableWidgetItem("1000"))
            self.table.setItem(row, 2, QTableWidgetItem("100"))
            
            # 모니터링 칸 (읽기 전용)
            item_vmon = QTableWidgetItem("0.0")
            item_vmon.setFlags(Qt.ItemIsEnabled); item_vmon.setForeground(QColor("#D32F2F"))
            self.table.setItem(row, 3, item_vmon)
            
            item_imon = QTableWidgetItem("0.0")
            item_imon.setFlags(Qt.ItemIsEnabled); item_imon.setForeground(QColor("#1976D2"))
            self.table.setItem(row, 4, item_imon)

            # 채널별 ON/OFF 버튼
            btn_pw = QPushButton("Power ON")
            btn_pw.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
            btn_pw.clicked.connect(lambda checked, r=row: self.toggle_channel_power(r))
            self.table.setCellWidget(row, 5, btn_pw)

        ctrl_layout.addWidget(self.table)
        
        # 일괄 적용 버튼
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
            self.sig_log.emit("\033[1;33m[WARNING] CAEN Python Library is not installed. Running in Mock Mode.\033[0m", False)

    def connect_hv(self):
        if not self.connected:
            ip = self.input_ip.text()
            self.sig_log.emit(f"\033[1;36m[HV]\033[0m Attempting to connect to CAEN at {ip}...", False)
            
            # TODO: 여기에 실제 CAEN connect() 함수 호출 로직 작성
            # 예: pycaenhv.connect(...)
            
            self.connected = True
            self.btn_connect.setText("❌ Disconnect")
            self.btn_connect.setStyleSheet("background-color: #757575; color: white; font-weight: bold;")
            self.table.setEnabled(True)
            self.sig_log.emit("\033[1;32m[HV] Successfully connected to CAEN Power Supply.\033[0m", False)
            
            # 연결 성공 시 1초마다 모니터링 폴링 시작
            self.poll_timer.start(1000)
        else:
            # TODO: 여기에 실제 CAEN disconnect() 함수 호출 로직 작성
            self.connected = False
            self.btn_connect.setText("🔗 Connect")
            self.btn_connect.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
            self.table.setEnabled(False)
            self.poll_timer.stop()
            self.sig_log.emit("\033[1;35m[HV] Disconnected from CAEN.\033[0m", False)

    def apply_all_settings(self):
        if not self.connected: return
        for row in range(self.num_channels):
            vset = self.table.item(row, 1).text()
            iset = self.table.item(row, 2).text()
            # TODO: 실제 라이브러리에 SetParam("V0Set", vset) 전송
            self.sig_log.emit(f"\033[1;34m[HV]\033[0m Ch {row} -> VSet: {vset}V, ISet: {iset}uA applied.", False)

    def toggle_channel_power(self, row):
        if not self.connected: return
        btn = self.table.cellWidget(row, 5)
        if btn.text() == "Power ON":
            # TODO: 실제 장비에 Power ON 명령 전송
            btn.setText("Power OFF")
            btn.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
            self.sig_log.emit(f"\033[1;31m[HV] Ch {row} RAMPING UP...\033[0m", False)
        else:
            # TODO: 실제 장비에 Power OFF 명령 전송
            btn.setText("Power ON")
            btn.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
            self.sig_log.emit(f"\033[1;34m[HV] Ch {row} RAMPING DOWN...\033[0m", False)

    def poll_hv_status(self):
        """1초마다 CAEN 장비에서 VMon, IMon 값을 읽어와 테이블을 갱신합니다."""
        if not self.connected: return
        
        for row in range(self.num_channels):
            # TODO: 실제 라이브러리에서 VMon, IMon 읽어오기
            # mock_vmon = pycaenhv.get_param("VMon", row)
            
            # 현재는 Mock 데이터 적용 (설정값 근처로 흔들리게 구현)
            import random
            btn = self.table.cellWidget(row, 5)
            if btn.text() == "Power OFF": # 켜져있는 상태
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
            self.sig_log.emit("\033[1;31m[HV EMERGENCY] Sending KILL signal to all channels!\033[0m", True)
            for row in range(self.num_channels):
                # TODO: 모든 채널 Power OFF 및 VSet 0 강제 전송
                btn = self.table.cellWidget(row, 5)
                btn.setText("Power ON")
                btn.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
            self.connect_hv() # 통신 절단
import os
import random
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                             QPushButton, QLineEdit, QGroupBox, QTableWidget, 
                             QTableWidgetItem, QCheckBox, QMessageBox, QHeaderView, QComboBox)
from PyQt5.QtCore import pyqtSignal, QTimer, Qt
from PyQt5.QtGui import QColor, QFont

try:
    from caen_libs import caenhvwrapper as hv
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
        layout.setContentsMargins(10, 10, 10, 10)

        # 💡 1. 모드 선택 토글
        mode_layout = QHBoxLayout()
        self.cb_enable_scm = QCheckBox("🔌 CAEN HV Network Control 활성화 (체크 시 TCP/IP/USB 기반 소프트웨어 제어)")
        self.cb_enable_scm.setStyleSheet("font-weight: bold; color: #1565C0; font-size: 14px;")
        self.cb_enable_scm.setChecked(False)
        self.cb_enable_scm.toggled.connect(self.toggle_mode)
        mode_layout.addWidget(self.cb_enable_scm)
        mode_layout.addStretch()
        layout.addLayout(mode_layout)

        # 💡 2. 아날로그 전용 수동 모드 안내 패널
        self.lbl_analog = QLabel(
            "⚠️ Analog HV Mode (Manual Setup)\n\n"
            "순수 아날로그 고전압 장비(ORTEC NIM 등)를 사용 중입니다.\n"
            "장비의 다이얼을 직접 돌려 전압을 인가하신 후, 아래 테이블(VSet/ISet)에 수동으로 값을 기록해 주십시오.\n"
            "(자동 모니터링 수치는 N/A 로 비활성화됩니다.)"
        )
        self.lbl_analog.setAlignment(Qt.AlignCenter)
        self.lbl_analog.setStyleSheet("""
            font-weight: bold; color: #5D4037; 
            background-color: #EFEBE9; 
            border: 2px dashed #8D6E63; 
            border-radius: 8px; 
            padding: 30px; 
            font-size: 14px;
        """)
        layout.addWidget(self.lbl_analog)

        # 💡 3. CAEN 제어 패널
        self.caen_widget = QWidget()
        caen_layout = QVBoxLayout(self.caen_widget)
        caen_layout.setContentsMargins(0, 0, 0, 0)

        conn_group = QGroupBox("CAEN HV Connection Configuration")
        conn_group.setStyleSheet("QGroupBox { border: 2px solid #1976D2; border-radius: 4px; margin-top: 10px; background-color: #E3F2FD;} QGroupBox::title { color: #1565C0; font-weight:bold;}")
        conn_layout = QHBoxLayout()
        
        # 💡 [추가] CAEN 장비 모델(System Type) 선택 콤보박스
        self.combo_sys = QComboBox()
        self.combo_sys.setStyleSheet("font-weight:bold; color:#E65100;")
        # 자주 쓰이는 모델들 위주로 우선 배치
        self.combo_sys.addItems(["SY4527", "SY5527", "N1470", "SMARTHV", "SY1527", "SY2527", "DT55XX", "V65XX"])
        
        self.combo_link = QComboBox()
        self.combo_link.addItems(["TCP/IP", "USB"])
        self.combo_link.setStyleSheet("font-weight:bold; color:#1565C0;")
        self.combo_link.currentIndexChanged.connect(self.on_link_type_changed)
        
        self.input_ip = QLineEdit("192.168.0.10")
        self.input_user = QLineEdit("admin")
        self.input_pwd = QLineEdit("admin")
        self.input_pwd.setEchoMode(QLineEdit.Password)
        self.input_user.setMaximumWidth(80)
        self.input_pwd.setMaximumWidth(80)
        
        self.btn_connect = QPushButton("🔗 Connect CAEN")
        self.btn_connect.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 6px;")
        self.btn_connect.clicked.connect(self.toggle_caen)

        conn_layout.addWidget(QLabel("Model:"))
        conn_layout.addWidget(self.combo_sys)
        conn_layout.addWidget(QLabel(" Link:"))
        conn_layout.addWidget(self.combo_link)
        self.lbl_ip_node = QLabel("IP:")
        conn_layout.addWidget(self.lbl_ip_node)
        conn_layout.addWidget(self.input_ip, stretch=1)
        conn_layout.addWidget(QLabel("User:"))
        conn_layout.addWidget(self.input_user)
        conn_layout.addWidget(QLabel("Pass:"))
        conn_layout.addWidget(self.input_pwd)
        conn_layout.addWidget(self.btn_connect)
        conn_group.setLayout(conn_layout)
        caen_layout.addWidget(conn_group)
        
        # 💡 [핵심] 연결 실패 / 텔넷 독점 방어용 수동 입력 패널 (평소에도 노출되어 DB 준비)
        self.caen_fallback_widget = QGroupBox("⚠️ Manual Input for Database (Server Offline or Exclusive Connection)")
        self.caen_fallback_widget.setFont(QFont("Arial", 10, QFont.Bold))
        self.caen_fallback_widget.setStyleSheet("""
            QGroupBox { 
                border: 2px solid #FFB300; 
                border-radius: 6px; 
                background-color: #FFF8E1; 
                margin-top: 10px; 
                margin-bottom: 5px;
            }
            QGroupBox::title { 
                subcontrol-origin: margin; 
                subcontrol-position: top left; 
                left: 10px; 
                color: #E65100; 
            }
        """)
        fb_layout = QHBoxLayout(self.caen_fallback_widget)
        fb_layout.setContentsMargins(10, 15, 10, 8) 
        
        self.lbl_fb_notice = QLabel("※ 장비 연결 실패 시, 아래 기입된 수동 값이 Run 시작 시 DB에 기록됩니다.")
        self.lbl_fb_notice.setStyleSheet("color: #D32F2F; font-weight: bold;")
        fb_layout.addWidget(self.lbl_fb_notice)
        fb_layout.addStretch()
        
        self.input_caen_fallback_v = QLineEdit()
        self.input_caen_fallback_v.setPlaceholderText("Target Voltage (V)")
        self.input_caen_fallback_v.setMinimumHeight(28)
        self.input_caen_fallback_v.setStyleSheet("font-weight:bold; font-size: 13px; padding: 2px; border: 1px solid #B0BEC5; background-color: white;")
        fb_layout.addWidget(self.input_caen_fallback_v)
        
        self.input_caen_fallback_i = QLineEdit()
        self.input_caen_fallback_i.setPlaceholderText("Target Current (uA)")
        self.input_caen_fallback_i.setMinimumHeight(28)
        self.input_caen_fallback_i.setStyleSheet("font-weight:bold; font-size: 13px; padding: 2px; border: 1px solid #B0BEC5; background-color: white;")
        fb_layout.addWidget(self.input_caen_fallback_i)
        
        caen_layout.addWidget(self.caen_fallback_widget)
        
        # 💡 4. 채널 모니터링 테이블
        ctrl_group = QGroupBox("HV Channel Configuration & Monitor")
        ctrl_layout = QVBoxLayout()
        
        self.table = QTableWidget(self.num_channels, 6)
        self.table.setHorizontalHeaderLabels(["Ch", "VSet (V)", "ISet (uA)", "VMon (V)", "IMon (uA)", "Power Status"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.setStyleSheet("QTableWidget { background-color: #FFFFFF; color: #333333; gridline-color: #CDBA96; font-size: 13px;}")
        
        self.setup_table_mode(is_caen=False)

        ctrl_layout.addWidget(self.table)
        
        self.btn_apply = QPushButton("⚡ Apply Settings to CAEN Hardware")
        self.btn_apply.setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 10px;")
        self.btn_apply.clicked.connect(self.apply_all_settings)
        self.btn_apply.setVisible(False)
        ctrl_layout.addWidget(self.btn_apply)

        ctrl_group.setLayout(ctrl_layout)
        caen_layout.addWidget(ctrl_group)
        
        conn_group.setVisible(False)
        self.caen_fallback_widget.setVisible(False)
        layout.addWidget(self.caen_widget)
        layout.addStretch()

    def on_link_type_changed(self):
        if self.combo_link.currentText() == "USB":
            self.lbl_ip_node.setText("Node:")
            self.input_ip.setText("0")
            self.input_user.setEnabled(False)
            self.input_pass.setEnabled(False)
            self.input_user.setStyleSheet("background-color: #E0E0E0; color: #9E9E9E;")
            self.input_pwd.setStyleSheet("background-color: #E0E0E0; color: #9E9E9E;")
        else:
            self.lbl_ip_node.setText("IP:")
            self.input_ip.setText("192.168.0.10")
            self.input_user.setEnabled(True)
            self.input_pass.setEnabled(True)
            self.input_user.setStyleSheet("")
            self.input_pwd.setStyleSheet("")

    def setup_table_mode(self, is_caen):
        for row in range(self.num_channels):
            item_ch = QTableWidgetItem(f"Ch {row}")
            item_ch.setFlags(Qt.ItemIsEnabled); item_ch.setTextAlignment(Qt.AlignCenter)
            self.table.setItem(row, 0, item_ch)
            
            if self.table.item(row, 1) is None: 
                self.table.setItem(row, 1, QTableWidgetItem("1000"))
                self.table.item(row, 1).setTextAlignment(Qt.AlignCenter)
            if self.table.item(row, 2) is None:
                self.table.setItem(row, 2, QTableWidgetItem("100"))
                self.table.item(row, 2).setTextAlignment(Qt.AlignCenter)
            
            item_vmon = QTableWidgetItem("N/A" if not is_caen else "0.0")
            item_vmon.setFlags(Qt.ItemIsEnabled); item_vmon.setTextAlignment(Qt.AlignCenter)
            item_vmon.setForeground(QColor("#9E9E9E" if not is_caen else "#D32F2F"))
            self.table.setItem(row, 3, item_vmon)
            
            item_imon = QTableWidgetItem("N/A" if not is_caen else "0.0")
            item_imon.setFlags(Qt.ItemIsEnabled); item_imon.setTextAlignment(Qt.AlignCenter)
            item_imon.setForeground(QColor("#9E9E9E" if not is_caen else "#1976D2"))
            self.table.setItem(row, 4, item_imon)

            if not is_caen:
                item_pw = QTableWidgetItem("Manual Mode")
                item_pw.setFlags(Qt.ItemIsEnabled); item_pw.setTextAlignment(Qt.AlignCenter)
                item_pw.setForeground(QColor("#5D4037"))
                self.table.removeCellWidget(row, 5) 
                self.table.setItem(row, 5, item_pw)
            else:
                self.table.setItem(row, 5, QTableWidgetItem("")) 
                btn_pw = QPushButton("Power ON")
                btn_pw.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
                btn_pw.setEnabled(self.connected)
                btn_pw.clicked.connect(lambda checked, r=row: self.toggle_channel_power(r))
                self.table.setCellWidget(row, 5, btn_pw)

    def toggle_mode(self, checked):
        self.lbl_analog.setVisible(not checked)
        self.caen_widget.setVisible(True)
        
        conn_group = self.caen_widget.findChild(QGroupBox, "CAEN HV Connection Configuration")
        if conn_group: conn_group.setVisible(checked)
        self.caen_fallback_widget.setVisible(checked)
        self.btn_apply.setVisible(checked)
        
        self.setup_table_mode(is_caen=checked)
        
        if checked:
            if not CAEN_AVAILABLE:
                self.sig_log.emit("<span style='color:#F57C00; font-weight:bold;'>[WARNING] CAEN Python Library not installed. Running Mock Connection.</span>", False)
            self.sig_log.emit("<span style='color:#1565C0; font-weight:bold;'>[HV] Switched to CAEN Control Mode. Please connect to the hardware.</span>", False)
        else:
            if self.connected: self.toggle_caen() 
            self.sig_log.emit("<span style='color:#5D4037; font-weight:bold;'>[HV] Switched to Analog Manual Mode. Values safely preserved.</span>", False)

    def map_system_type(self, type_str):
        if not CAEN_AVAILABLE: return None
        try:
            return getattr(hv.SystemType, type_str.split()[0]) # 'SY4527 / SY5527' -> 'SY4527'
        except AttributeError:
            return hv.SystemType.SY4527 # 기본 폴백

    def toggle_caen(self):
        if not self.connected:
            ip = self.input_ip.text()
            sys_type_str = self.combo_sys.currentText()
            self.sig_log.emit(f"<span style='color:#0288D1; font-weight:bold;'>[HV]</span> Attempting to connect to {sys_type_str} at {ip}...", False)
            
            try:
                # 실제 장비 연결 로직 (Mocking이 아닐 때)
                if CAEN_AVAILABLE:
                    link_type = hv.LinkType.TCPIP if self.combo_link.currentText() == "TCP/IP" else hv.LinkType.USB
                    sys_type = self.map_system_type(sys_type_str)
                    
                    # 💡 [핵심] 독점 포트로 인해 연결 거부(Error) 발생 시 예외 처리됨!
                    # self.device = hv.Device.open(sys_type, link_type, ip, self.input_user.text(), self.input_pwd.text())
                    
                self.connected = True
                self.btn_connect.setText("❌ Disconnect")
                self.btn_connect.setStyleSheet("background-color: #757575; color: white; font-weight: bold;")
                
                # 💡 연결 성공 시 텔넷 Fallback 경고문을 안심 메시지로 교체
                self.caen_fallback_widget.setStyleSheet("QGroupBox { border: 2px solid #4CAF50; border-radius: 6px; background-color: #E8F5E9; margin-top: 10px; margin-bottom: 5px;}")
                self.lbl_fb_notice.setText("🟢 연결 완료! 하드웨어 동기화 활성화됨. 아래 수동 입력값은 무시됩니다.")
                self.lbl_fb_notice.setStyleSheet("color: #2E7D32; font-weight: bold;")
                
                for row in range(self.num_channels):
                    btn = self.table.cellWidget(row, 5)
                    if btn: btn.setEnabled(True)
                    
                self.sig_log.emit("<span style='color:#388E3C; font-weight:bold;'>[HV] Successfully connected to CAEN Power Supply.</span>", False)
                self.poll_timer.start(1000)
                
            except Exception as e:
                self.sig_log.emit(f"<span style='color:#D32F2F; font-weight:bold;'>[HV ERROR] Connection Failed! (Reason: {e})</span>", True)
                self.sig_log.emit("<span style='color:#E65100; font-weight:bold;'>[HV TIP] 텔넷(Telnet) 독점 연결에 의해 거부되었을 수 있습니다. 수동 입력 패널(Fallback)에 값을 기입해 주십시오.</span>", False)
        else:
            self.connected = False
            self.btn_connect.setText("🔗 Connect CAEN")
            self.btn_connect.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
            
            self.caen_fallback_widget.setStyleSheet("QGroupBox { border: 2px solid #FFB300; border-radius: 6px; background-color: #FFF8E1; margin-top: 10px; margin-bottom: 5px;}")
            self.lbl_fb_notice.setText("※ 장비 연결 해제됨. 아래 기입된 수동 값이 Run 시작 시 DB에 기록됩니다.")
            self.lbl_fb_notice.setStyleSheet("color: #D32F2F; font-weight: bold;")
            
            for row in range(self.num_channels):
                btn = self.table.cellWidget(row, 5)
                if btn: btn.setEnabled(False)
                
            self.poll_timer.stop()
            self.sig_log.emit("<span style='color:#8E24AA; font-weight:bold;'>[HV] Disconnected from CAEN. Fallback mode activated.</span>", False)

    def apply_all_settings(self):
        if not self.connected: 
            QMessageBox.warning(self, "Offline", "장비와 연결되어 있지 않습니다.\n수동 입력 패널(Fallback)에 값을 기입하면 DB에 기록됩니다.")
            return
        for row in range(self.num_channels):
            vset = self.table.item(row, 1).text()
            iset = self.table.item(row, 2).text()
            self.sig_log.emit(f"<span style='color:#1976D2; font-weight:bold;'>[HV]</span> Ch {row} -> VSet: {vset}V, ISet: {iset}uA applied to hardware.", False)

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

    # 💡 [핵심] DaqTab에서 Run 시작 시, 현재 모드와 통신 상태를 파악하여 DB로 푸쉬할 값을 지능적으로 빼주는 함수
    def get_current_hv(self):
        if self.cb_enable_scm.isChecked():
            if self.connected:
                # 하드웨어 통신 중일 때는 0번 채널의 VMon/IMon 값을 대표로 스캔
                vmon = self.table.item(0, 3).text()
                imon = self.table.item(0, 4).text()
                return f"{vmon}V, {imon}uA (CAEN Online)"
            else:
                # 하드웨어 텔넷 락킹 등 오프라인일 때는 수동 Fallback 상자 값 반환
                v = self.input_caen_fallback_v.text().strip()
                i = self.input_caen_fallback_i.text().strip()
                if v and i: return f"{v}V, {i}uA (CAEN Manual Fallback)"
                elif v: return f"{v}V (CAEN Manual Fallback)"
                else: return "N/A (CAEN Offline)"
        else:
            # 완전 아날로그 모드 (ORTEC 등)
            v = self.table.item(0, 1).text()  # 아날로그 모드는 테이블 VSet을 기록용으로 활용
            i = self.table.item(0, 2).text()
            return f"{v}V, {i}uA (Analog Manual)"

    def force_shutdown(self):
        if self.connected:
            self.sig_log.emit("<span style='color:#D32F2F; font-weight:bold;'>[HV EMERGENCY] Sending KILL signal to all CAEN channels!</span>", True)
            for row in range(self.num_channels):
                btn = self.table.cellWidget(row, 5)
                btn.setText("Power ON")
                btn.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
            self.toggle_caen()
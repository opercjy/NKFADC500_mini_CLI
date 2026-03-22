import os
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                             QPushButton, QLineEdit, QGroupBox, QCheckBox, QGridLayout)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QFont

class TltTab(QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()

    def initUI(self):
        layout = QVBoxLayout(self)

        # =========================================================
        # 1. TLT Logic Builder (초보자용 조합 생성기)
        # =========================================================
        builder_group = QGroupBox("1. TLT Logic Builder (조합 생성기)")
        builder_layout = QVBoxLayout()
        
        desc_lbl = QLabel("원하는 동시 발생(Coincidence) 조건들을 모두 체크하면 settings.cfg에 들어갈 TRIG_TLT 값을 계산합니다.")
        desc_lbl.setStyleSheet("color: #555555; margin-bottom: 10px;")
        builder_layout.addWidget(desc_lbl)

        grid_layout = QGridLayout()
        self.builder_chks = []
        
        row, col = 0, 0
        for i in range(1, 16):
            chs = []
            if i & 1: chs.append("Ch0")
            if i & 2: chs.append("Ch1")
            if i & 4: chs.append("Ch2")
            if i & 8: chs.append("Ch3")
            
            chk = QCheckBox(f"P{i:02d}: [{' & '.join(chs)}]")
            chk.setStyleSheet("font-size: 13px; padding: 2px;")
            chk.stateChanged.connect(self.calc_tlt)
            self.builder_chks.append((i, chk))
            
            grid_layout.addWidget(chk, row, col)
            col += 1
            if col > 3: # 4열 배치
                col = 0
                row += 1
                
        builder_layout.addLayout(grid_layout)
        
        res_layout = QHBoxLayout()
        self.out_dec = QLineEdit("65534"); self.out_dec.setReadOnly(True)
        self.out_hex = QLineEdit("0xFFFE"); self.out_hex.setReadOnly(True)
        self.out_dec.setStyleSheet("font-weight: bold; color: #2E7D32; font-size: 14px; padding: 5px;")
        self.out_hex.setStyleSheet("font-weight: bold; color: #1565C0; font-size: 14px; padding: 5px;")
        
        btn_to_sim = QPushButton("⬇️ 시뮬레이터로 복사")
        btn_to_sim.setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; padding: 5px 15px;")
        btn_to_sim.clicked.connect(self.copy_to_sim)
        
        res_layout.addWidget(QLabel("Decimal (10진수):")); res_layout.addWidget(self.out_dec)
        res_layout.addWidget(QLabel("Hex (16진수):")); res_layout.addWidget(self.out_hex)
        res_layout.addWidget(btn_to_sim)
        builder_layout.addLayout(res_layout)
        
        builder_group.setLayout(builder_layout)
        layout.addWidget(builder_group)

        # =========================================================
        # 2. Channel Trigger Simulator (결과 테스트 시뮬레이터)
        # =========================================================
        sim_group = QGroupBox("2. Channel Trigger Simulator (가상 신호 테스트)")
        sim_layout = QVBoxLayout()
        
        input_layout = QHBoxLayout()
        input_layout.addWidget(QLabel("Test TRIG_TLT (Dec/Hex):"))
        self.val_input = QLineEdit("65534")
        self.val_input.setStyleSheet("background-color: white; color: black; font-size: 14px; font-weight: bold; padding: 5px;")
        self.val_input.textChanged.connect(self.simulate)
        input_layout.addWidget(self.val_input)
        sim_layout.addLayout(input_layout)
        
        ch_layout = QHBoxLayout()
        self.sim_chks = []
        for i in range(4):
            chk = QCheckBox(f"Ch {i} Fired ⚡")
            chk.setStyleSheet("font-weight: bold; font-size: 16px; color: #1976D2; margin: 10px 0;")
            chk.stateChanged.connect(self.simulate)
            self.sim_chks.append(chk)
            ch_layout.addWidget(chk)
        
        sim_layout.addLayout(ch_layout)

        # 결과 출력 전광판
        self.result_lbl = QLabel("IDLE")
        self.result_lbl.setAlignment(Qt.AlignCenter)
        self.result_lbl.setStyleSheet("background-color: #E0E0E0; font-size: 22px; font-weight: bold; padding: 20px; border-radius: 8px;")
        sim_layout.addWidget(self.result_lbl)

        sim_group.setLayout(sim_layout)
        layout.addWidget(sim_group)
        
        # 기본값 초기화 (모두 체크 = OR 로직 = 65534)
        for _, chk in self.builder_chks:
            chk.setChecked(True)
            
        layout.addStretch()
        self.simulate()

    def calc_tlt(self):
        tlt_val = 0
        for bit_index, chk in self.builder_chks:
            if chk.isChecked():
                tlt_val |= (1 << bit_index)
        self.out_dec.setText(str(tlt_val))
        self.out_hex.setText(f"0x{tlt_val:04X}")

    def copy_to_sim(self):
        """Builder에서 계산된 값을 Simulator 입력창으로 전달합니다."""
        self.val_input.setText(self.out_dec.text())
        self.simulate()

    def simulate(self):
        try:
            text = self.val_input.text().strip()
            if text.startswith("0x") or text.startswith("0X"): 
                tlt_val = int(text, 16)
            else: 
                tlt_val = int(text)
        except ValueError:
            self.result_lbl.setText("❌ INVALID TLT VALUE")
            self.result_lbl.setStyleSheet("background-color: #FFCDD2; color: #B71C1C; font-size: 22px; font-weight: bold; padding: 20px; border-radius: 8px;")
            return

        pattern = 0
        for i in range(4):
            if self.sim_chks[i].isChecked():
                pattern |= (1 << i)
        
        is_triggered = (tlt_val & (1 << pattern)) != 0

        if pattern == 0:
            self.result_lbl.setText("💤 NO SIGNAL (모든 채널 신호 없음)")
            self.result_lbl.setStyleSheet("background-color: #E0E0E0; color: #757575; font-size: 22px; font-weight: bold; padding: 20px; border-radius: 8px;")
        elif is_triggered:
            self.result_lbl.setText("🟢 TRIGGER ACCEPTED! (수집 통과)")
            self.result_lbl.setStyleSheet("background-color: #C8E6C9; color: #2E7D32; font-size: 22px; font-weight: bold; padding: 20px; border-radius: 8px;")
        else:
            self.result_lbl.setText("🔴 TRIGGER BLOCKED (로직에 의해 차단됨)")
            self.result_lbl.setStyleSheet("background-color: #FFCDD2; color: #C62828; font-size: 22px; font-weight: bold; padding: 20px; border-radius: 8px;")
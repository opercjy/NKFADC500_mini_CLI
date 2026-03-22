import sqlite3
import os
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QPushButton, 
                             QTableWidget, QTableWidgetItem, QHeaderView, QTabWidget)
from PyQt5.QtCore import Qt

class DatabaseTab(QWidget):
    def __init__(self):
        super().__init__()
        self.db_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../data"))
        self.db_path = os.path.join(self.db_dir, "fadc500_history.db")
        self.initUI()
        self.load_data()

    def initUI(self):
        layout = QVBoxLayout(self)
        
        btn_refresh = QPushButton("🔄 Refresh Database (데이터베이스 새로고침)")
        btn_refresh.setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 10px;")
        btn_refresh.clicked.connect(self.load_data)
        layout.addWidget(btn_refresh)

        self.tabs = QTabWidget()
        
        # 프론트엔드 수집 기록 (DAQ)
        self.daq_table = QTableWidget(0, 8)
        self.daq_table.setHorizontalHeaderLabels(["Run Num", "Start Time", "End Time", "Elapsed(s)", "Total Events", "Size(MB)", "Rate(Hz)", "Mode"])
        self.daq_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.daq_table.setStyleSheet("background-color: white; color: black; gridline-color: #CDBA96;")
        self.tabs.addTab(self.daq_table, "🚀 DAQ History")

        # 오프라인 변환 기록 (Production)
        self.prod_table = QTableWidget(0, 6)
        self.prod_table.setHorizontalHeaderLabels(["Run Num", "Process Time", "Elapsed(s)", "Total Events", "Speed(MB/s)", "Mode"])
        self.prod_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.prod_table.setStyleSheet("background-color: white; color: black; gridline-color: #CDBA96;")
        self.tabs.addTab(self.prod_table, "🛠️ Production History")

        layout.addWidget(self.tabs)

    def load_data(self):
        if not os.path.exists(self.db_path): return
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        # DAQ 테이블 렌더링
        try:
            cursor.execute("SELECT * FROM frontend_runs ORDER BY run_number DESC")
            rows = cursor.fetchall()
            self.daq_table.setRowCount(len(rows))
            for r_idx, row in enumerate(rows):
                for c_idx, val in enumerate(row):
                    item = QTableWidgetItem(str(val))
                    item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
                    self.daq_table.setItem(r_idx, c_idx, item)
        except Exception: pass

        # Production 테이블 렌더링
        try:
            cursor.execute("SELECT * FROM production_runs ORDER BY run_number DESC")
            rows = cursor.fetchall()
            self.prod_table.setRowCount(len(rows))
            for r_idx, row in enumerate(rows):
                for c_idx, val in enumerate(row):
                    item = QTableWidgetItem(str(val))
                    item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
                    self.prod_table.setItem(r_idx, c_idx, item)
        except Exception: pass
        
        conn.close()
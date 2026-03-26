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
        layout.setContentsMargins(10, 10, 10, 10)
        
        # 💡 새로고침 버튼 (테마 적용)
        btn_refresh = QPushButton("🔄 Refresh Database (최신 런 이력 불러오기)")
        btn_refresh.setStyleSheet("""
            background-color: #4CAF50; 
            color: white; 
            font-weight: bold; 
            padding: 10px;
            border-radius: 4px;
        """)
        btn_refresh.clicked.connect(self.load_data)
        layout.addWidget(btn_refresh)

        self.tabs = QTabWidget()
        
        # 💡 공통 테이블 스타일 (화이트/베이지 테마 일치)
        table_style = """
            QTableWidget { 
                background-color: #FFFFFF; 
                color: #333333; 
                gridline-color: #CDBA96; 
                font-size: 13px; 
            }
            QHeaderView::section { 
                background-color: #E8E2D2; 
                color: #333333; 
                font-weight: bold; 
                border: 1px solid #CDBA96; 
                padding: 5px; 
            }
        """
        
        # =====================================================================
        # 💡 1. DAQ History 테이블 (새 컬럼 반영)
        # =====================================================================
        self.daq_table = QTableWidget(0, 11)
        self.daq_table.setHorizontalHeaderLabels([
            "Run(UID)", "Start Time", "Elapsed(s)", "Events", 
            "Size(MB)", "Rate(Hz)", "Mode", "Config", 
            "Quality", "Comments", "End Time"
        ])
        self.daq_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        # 코멘트(Comments) 열은 남은 공간을 꽉 채우도록 렌더링
        self.daq_table.horizontalHeader().setSectionResizeMode(9, QHeaderView.Stretch)
        self.daq_table.setStyleSheet(table_style)
        self.tabs.addTab(self.daq_table, "🚀 DAQ History")

        # =====================================================================
        # 💡 2. Production History 테이블 (새 컬럼 반영)
        # =====================================================================
        self.prod_table = QTableWidget(0, 8)
        self.prod_table.setHorizontalHeaderLabels([
            "Run", "Process Time", "Elapsed(s)", "Events", 
            "Speed(MB/s)", "Mode", "Input File", "Output File"
        ])
        self.prod_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        # Output File 열은 남은 공간을 꽉 채우도록 렌더링
        self.prod_table.horizontalHeader().setSectionResizeMode(7, QHeaderView.Stretch)
        self.prod_table.setStyleSheet(table_style)
        self.tabs.addTab(self.prod_table, "🛠️ Production History")

        layout.addWidget(self.tabs)

    def load_data(self):
        if not os.path.exists(self.db_path): 
            return
            
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        # DAQ 테이블 로드
        try:
            # DatabaseManager에서 추가한 컬럼 순서대로 SELECT
            cursor.execute("""
                SELECT run_number, start_time, elapsed_sec, total_events, 
                       total_mb, avg_rate_hz, scan_mode, config_file, 
                       quality, comments, end_time 
                FROM frontend_runs 
                ORDER BY start_time DESC LIMIT 200
            """)
            rows = cursor.fetchall()
            self.daq_table.setRowCount(len(rows))
            for r_idx, row in enumerate(rows):
                for c_idx, val in enumerate(row):
                    item = QTableWidgetItem(str(val) if val is not None else "")
                    item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable) # 읽기 전용 방어
                    # 셀 텍스트 중앙 정렬 (코멘트 제외)
                    if c_idx != 9: 
                        item.setTextAlignment(Qt.AlignCenter)
                    self.daq_table.setItem(r_idx, c_idx, item)
        except Exception as e: 
            print(f"[DB Load Error - DAQ] {e}")

        # Production 테이블 로드
        try:
            cursor.execute("""
                SELECT run_number, process_time, elapsed_sec, total_events, 
                       speed_mbps, mode, input_file, output_file 
                FROM production_runs 
                ORDER BY process_time DESC LIMIT 200
            """)
            rows = cursor.fetchall()
            self.prod_table.setRowCount(len(rows))
            for r_idx, row in enumerate(rows):
                for c_idx, val in enumerate(row):
                    item = QTableWidgetItem(str(val) if val is not None else "")
                    item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
                    # 셀 텍스트 중앙 정렬 (입/출력 파일명 제외)
                    if c_idx not in [6, 7]: 
                        item.setTextAlignment(Qt.AlignCenter)
                    self.prod_table.setItem(r_idx, c_idx, item)
        except Exception as e: 
            print(f"[DB Load Error - PROD] {e}")
        
        conn.close()
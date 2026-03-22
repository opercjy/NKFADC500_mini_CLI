import sqlite3
import os
from datetime import datetime

class DatabaseManager:
    def __init__(self, db_name="fadc500_history.db"):
        self.db_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../data"))
        os.makedirs(self.db_dir, exist_ok=True)
        self.db_path = os.path.join(self.db_dir, db_name)
        self.init_tables()

    def init_tables(self):
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        cursor.execute('''CREATE TABLE IF NOT EXISTS frontend_runs (run_number INTEGER PRIMARY KEY, start_time TEXT, end_time TEXT, elapsed_sec REAL, total_events INTEGER, total_mb REAL, avg_rate_hz REAL, scan_mode TEXT)''')
        cursor.execute('''CREATE TABLE IF NOT EXISTS production_runs (run_number INTEGER PRIMARY KEY, process_time TEXT, elapsed_sec REAL, total_events INTEGER, speed_mbps REAL, mode TEXT)''')
        
        # 💡 [핵심] 기존 DB 파괴 없이 새로운 컬럼(품질, 코멘트, 서브런) 안전하게 추가
        try: cursor.execute("ALTER TABLE frontend_runs ADD COLUMN quality TEXT DEFAULT 'GOOD'")
        except: pass
        try: cursor.execute("ALTER TABLE frontend_runs ADD COLUMN comments TEXT DEFAULT ''")
        except: pass
        try: cursor.execute("ALTER TABLE frontend_runs ADD COLUMN subrun INTEGER DEFAULT 0")
        except: pass
        conn.commit(); conn.close()

    def insert_frontend_summary(self, run_num, subrun, start, end, elapsed, events, size_mb, rate, mode, quality, comments):
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        # Subrun 구분을 위해 run_number를 복합키 개념으로 덮어씀 (run_num * 1000 + subrun)
        uid = int(run_num) * 1000 + int(subrun)
        cursor.execute('''
            INSERT OR REPLACE INTO frontend_runs 
            (run_number, subrun, start_time, end_time, elapsed_sec, total_events, total_mb, avg_rate_hz, scan_mode, quality, comments)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (uid, subrun, start, end, elapsed, events, size_mb, rate, mode, quality, comments))
        conn.commit(); conn.close()

    def insert_production_summary(self, run_num, elapsed, events, speed, mode):
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        cursor.execute('''
            INSERT OR REPLACE INTO production_runs 
            (run_number, process_time, elapsed_sec, total_events, speed_mbps, mode)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (run_num, now, elapsed, events, speed, mode))
        conn.commit(); conn.close()
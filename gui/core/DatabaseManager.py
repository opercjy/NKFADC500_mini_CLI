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
        
        # [스키마 동적 확장] 기존 데이터 유지하며 새로운 컬럼 안전하게 추가
        try: cursor.execute("ALTER TABLE frontend_runs ADD COLUMN quality TEXT DEFAULT 'GOOD'")
        except: pass
        try: cursor.execute("ALTER TABLE frontend_runs ADD COLUMN comments TEXT DEFAULT ''")
        except: pass
        try: cursor.execute("ALTER TABLE frontend_runs ADD COLUMN subrun INTEGER DEFAULT 0")
        except: pass
        try: cursor.execute("ALTER TABLE frontend_runs ADD COLUMN config_file TEXT DEFAULT ''")
        except: pass
        try: cursor.execute("ALTER TABLE production_runs ADD COLUMN input_file TEXT DEFAULT ''")
        except: pass
        try: cursor.execute("ALTER TABLE production_runs ADD COLUMN output_file TEXT DEFAULT ''")
        except: pass
        conn.commit(); conn.close()

    def insert_frontend_summary(self, run_num, subrun, start, end, elapsed, events, size_mb, rate, mode, quality, comments, config_file):
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        uid = int(run_num) * 1000 + int(subrun)
        cursor.execute('''
            INSERT OR REPLACE INTO frontend_runs 
            (run_number, subrun, start_time, end_time, elapsed_sec, total_events, total_mb, avg_rate_hz, scan_mode, quality, comments, config_file)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (uid, subrun, start, end, elapsed, events, size_mb, rate, mode, quality, comments, config_file))
        conn.commit(); conn.close()

    def insert_production_summary(self, run_num, elapsed, events, speed, mode, in_file, out_file):
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        cursor.execute('''
            INSERT OR REPLACE INTO production_runs 
            (run_number, process_time, elapsed_sec, total_events, speed_mbps, mode, input_file, output_file)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ''', (run_num, now, elapsed, events, speed, mode, in_file, out_file))
        conn.commit(); conn.close()

    def get_latest_run_number(self):
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        cursor.execute("SELECT run_number FROM frontend_runs ORDER BY start_time DESC LIMIT 1")
        row = cursor.fetchone()
        conn.close()
        if row:
            return str(int(row[0] // 1000) + 1)
        return "101"
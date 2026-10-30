import os
import sqlite3
from datetime import datetime
from core.Paths import get_default_data_dir


class DatabaseManager:
    def __init__(self, data_dir=None, db_name="fadc500_history.db"):
        self.db_dir = os.path.abspath(data_dir) if data_dir else get_default_data_dir()
        os.makedirs(self.db_dir, exist_ok=True)
        self.db_path = os.path.join(self.db_dir, db_name)
        self.init_tables()

    def init_tables(self):
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.cursor()
            cursor.execute(
                """CREATE TABLE IF NOT EXISTS frontend_runs (
                    run_number INTEGER PRIMARY KEY,
                    start_time TEXT,
                    end_time TEXT,
                    elapsed_sec REAL,
                    total_events INTEGER,
                    total_mb REAL,
                    avg_rate_hz REAL,
                    scan_mode TEXT
                )"""
            )
            cursor.execute(
                """CREATE TABLE IF NOT EXISTS production_runs (
                    run_number INTEGER PRIMARY KEY,
                    process_time TEXT,
                    elapsed_sec REAL,
                    total_events INTEGER,
                    speed_mbps REAL,
                    mode TEXT
                )"""
            )

            # Backward-compatible schema migrations
            migrations = [
                "ALTER TABLE frontend_runs ADD COLUMN quality TEXT DEFAULT 'GOOD'",
                "ALTER TABLE frontend_runs ADD COLUMN comments TEXT DEFAULT ''",
                "ALTER TABLE frontend_runs ADD COLUMN subrun INTEGER DEFAULT 0",
                "ALTER TABLE frontend_runs ADD COLUMN config_file TEXT DEFAULT ''",
                "ALTER TABLE production_runs ADD COLUMN input_file TEXT DEFAULT ''",
                "ALTER TABLE production_runs ADD COLUMN output_file TEXT DEFAULT ''",
            ]
            for stmt in migrations:
                try:
                    cursor.execute(stmt)
                except sqlite3.OperationalError:
                    pass
            conn.commit()

    def insert_frontend_summary(
        self, run_num, subrun, start, end, elapsed, events, size_mb, rate, mode, quality, comments, config_file
    ):
        uid = int(run_num) * 1000 + int(subrun)
        with sqlite3.connect(self.db_path) as conn:
            conn.execute(
                """
                INSERT OR REPLACE INTO frontend_runs
                (run_number, subrun, start_time, end_time, elapsed_sec, total_events,
                 total_mb, avg_rate_hz, scan_mode, quality, comments, config_file)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (uid, int(subrun), start, end, elapsed, int(events), float(size_mb), float(rate), mode, quality, comments, config_file),
            )
            conn.commit()

    def insert_production_summary(self, run_num, elapsed, events, speed, mode, in_file, out_file):
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        with sqlite3.connect(self.db_path) as conn:
            conn.execute(
                """
                INSERT OR REPLACE INTO production_runs
                (run_number, process_time, elapsed_sec, total_events, speed_mbps, mode, input_file, output_file)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (int(run_num), now, float(elapsed), int(events), float(speed), mode, in_file, out_file),
            )
            conn.commit()

    def get_latest_run_number(self) -> str:
        with sqlite3.connect(self.db_path) as conn:
            row = conn.execute("SELECT run_number FROM frontend_runs ORDER BY start_time DESC LIMIT 1").fetchone()
        if row and row[0] is not None:
            return str(int(row[0] // 1000) + 1)
        return "101"

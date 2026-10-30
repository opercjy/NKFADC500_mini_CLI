import os
import re
import shutil
from datetime import datetime
from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QFileDialog,
    QGroupBox,
    QHBoxLayout,
    QInputDialog,
    QLineEdit,
    QProgressBar,
    QPushButton,
    QVBoxLayout,
    QWidget,
)
from core.DatabaseManager import DatabaseManager
from core.Paths import get_bin_path
from core.ProcessManager import ProcessManager
from widgets.path_controller import PathControllerWidget


def extract_run_number_from_filename(filename: str) -> int:
    stem = os.path.splitext(os.path.basename(filename))[0]
    m = re.match(r"^.+?_(\d+)(?:_(?:thr\d+|\d{3}))?$", stem, re.IGNORECASE)
    if m:
        return int(m.group(1))
    nums = re.findall(r"\d+", stem)
    return int(nums[-1]) if nums else 0


class ProductionTab(QWidget):
    sig_log = Signal(str, bool)
    sig_prod_finished = Signal()

    def __init__(self, data_dir, config_dir):
        super().__init__()
        self.data_dir = data_dir
        self.config_dir = config_dir
        self.db = DatabaseManager(data_dir=self.data_dir)
        self.start_time = None
        self.is_interactive_run = False
        self.final_stats = {"events": 0, "speed": 0.0}
        self.current_out_file = ""

        self.prod_manager = ProcessManager(self)
        self.prod_manager.log_signal.connect(lambda msg, is_err: self.sig_log.emit(msg, is_err))
        self.prod_manager.stat_signal.connect(self.update_progress)
        self.prod_manager.state_signal.connect(self.handle_state)
        self.initUI()

    def initUI(self):
        layout = QVBoxLayout(self)

        self.path_ctrl = PathControllerWidget("📁 Batch Output Path:", self.data_dir)
        layout.addWidget(self.path_ctrl)

        file_group = QGroupBox("1. Select Raw Data File (.dat)")
        file_layout = QHBoxLayout()
        self.input_file = QLineEdit()
        btn_browse = QPushButton("📂 Browse...")
        btn_browse.clicked.connect(self.browse_file)
        file_layout.addWidget(self.input_file)
        file_layout.addWidget(btn_browse)
        file_group.setLayout(file_layout)
        layout.addWidget(file_group)

        action_group = QGroupBox("2. Run Production")
        action_layout = QVBoxLayout()
        self.chk_wave = QCheckBox("Save Waveform (-w mode)")
        self.btn_batch = QPushButton("⚙️ Run Batch (고속 변환)")
        self.btn_batch.setStyleSheet("background-color: #673AB7; color: white; padding: 10px; font-weight:bold;")
        self.btn_batch.clicked.connect(self.run_batch)

        self.btn_inter = QPushButton("📈 Open Interactive Viewer (-d Mode)")
        self.btn_inter.setStyleSheet("background-color: #009688; color: white; padding: 10px; font-weight:bold;")
        self.btn_inter.clicked.connect(self.run_interactive)

        self.progress = QProgressBar()
        self.progress.setAlignment(Qt.AlignCenter)
        self.progress.setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }")

        action_layout.addWidget(self.chk_wave)
        action_layout.addWidget(self.btn_batch)
        action_layout.addWidget(self.btn_inter)
        action_layout.addWidget(self.progress)
        action_group.setLayout(action_layout)
        layout.addWidget(action_group)

        self.inter_group = QGroupBox("3. Interactive Controls (-d 전용)")
        self.inter_group.setEnabled(False)
        inter_layout = QHBoxLayout()
        btn_prev = QPushButton("⏮ Prev")
        btn_prev.clicked.connect(lambda: self.prod_manager.write_stdin("p"))
        btn_next = QPushButton("⏭ Next")
        btn_next.clicked.connect(lambda: self.prod_manager.write_stdin("n"))
        btn_jump = QPushButton("🔢 Jump")
        btn_jump.clicked.connect(self.cmd_jump)
        btn_quit = QPushButton("⏹ Quit")
        btn_quit.clicked.connect(lambda: self.prod_manager.write_stdin("q"))

        inter_layout.addWidget(btn_prev)
        inter_layout.addWidget(btn_next)
        inter_layout.addWidget(btn_jump)
        inter_layout.addWidget(btn_quit)
        self.inter_group.setLayout(inter_layout)
        layout.addWidget(self.inter_group)
        layout.addStretch()

    def browse_file(self):
        filename, _ = QFileDialog.getOpenFileName(self, "Select Raw File", self.data_dir, "Data Files (*.dat)")
        if filename:
            self.input_file.setText(filename)

    def run_batch(self):
        infile = self.input_file.text().strip()
        if not infile or not os.path.exists(infile):
            self.sig_log.emit(f"<span style='color:#D32F2F; font-weight:bold;'>[ERROR] Input .dat file not found: {infile}</span>", True)
            return

        self.is_interactive_run = False
        self.progress.setValue(0)
        self.start_time = datetime.now()
        self.final_stats = {"events": 0, "speed": 0.0}

        target_dir = self.path_ctrl.get_path()
        os.makedirs(target_dir, exist_ok=True)
        out_name = os.path.splitext(os.path.basename(infile))[0] + "_prod.root"
        self.current_out_file = os.path.join(target_dir, out_name)

        bin_path = get_bin_path("production_nkfadc_500")
        args = [infile, "-o", self.current_out_file]
        if self.chk_wave.isChecked():
            args.append("-w")
        self.prod_manager.start_process(bin_path, args)

    def run_interactive(self):
        infile = self.input_file.text().strip()
        if not infile or not os.path.exists(infile):
            self.sig_log.emit(f"<span style='color:#D32F2F; font-weight:bold;'>[ERROR] Input .dat file not found: {infile}</span>", True)
            return

        self.is_interactive_run = True
        self.start_time = None
        bin_path = get_bin_path("production_nkfadc_500")
        self.prod_manager.start_process(bin_path, [infile, "-d"])

    def cmd_jump(self):
        num, ok = QInputDialog.getInt(self, "Jump to Event", "이동할 이벤트 번호:", 0, 0, 1000000000)
        if ok:
            self.prod_manager.write_stdin(f"j\n{num}")

    def update_progress(self, stats):
        if "progress" in stats:
            self.progress.setValue(int(stats["progress"]))
        if "final_events" in stats:
            self.final_stats["events"] = stats["final_events"]
        if "final_speed" in stats:
            self.final_stats["speed"] = stats["final_speed"]

    def handle_state(self, is_running):
        self.btn_batch.setEnabled(not is_running)
        self.btn_inter.setEnabled(not is_running)
        self.inter_group.setEnabled(is_running and self.is_interactive_run)

        if not is_running and self.start_time is not None:
            self.progress.setValue(100)
            elapsed = (datetime.now() - self.start_time).total_seconds()

            infile_path = self.input_file.text().strip()
            in_filename = os.path.basename(infile_path)
            run_num = extract_run_number_from_filename(in_filename)
            out_filename = os.path.basename(self.current_out_file)

            # Fallback move if an older binary created <infile>_prod.root in-place
            orig_out_path = os.path.splitext(infile_path)[0] + "_prod.root"
            if not os.path.exists(self.current_out_file) and os.path.exists(orig_out_path):
                try:
                    shutil.move(orig_out_path, self.current_out_file)
                except Exception as e:
                    self.sig_log.emit(
                        f"<span style='color:#EF5350; font-weight:bold;'>[ERROR] Failed to move output file: {e}</span>",
                        True,
                    )

            if self.final_stats.get("speed", 0.0) <= 0.0 and elapsed > 0.0 and os.path.exists(infile_path):
                self.final_stats["speed"] = round((os.path.getsize(infile_path) / 1048576.0) / elapsed, 2)

            self.db.insert_production_summary(
                run_num=run_num,
                elapsed=round(elapsed, 2),
                events=self.final_stats.get("events", 0),
                speed=round(float(self.final_stats.get("speed", 0.0)), 2),
                mode="Waveform" if self.chk_wave.isChecked() else "Fast Physics",
                in_file=in_filename,
                out_file=out_filename,
            )

            self.sig_log.emit(
                f"<span style='color:#2E7D32; font-weight:bold;'>[DB] Production Summary saved. Output: {out_filename}</span>",
                False,
            )
            self.sig_prod_finished.emit()
            self.start_time = None

    def force_abort(self):
        self.prod_manager.stop_process()

    def is_running(self):
        return self.prod_manager.is_running()

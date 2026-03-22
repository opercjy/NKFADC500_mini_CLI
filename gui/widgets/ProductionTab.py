import os
from datetime import datetime
from core.DatabaseManager import DatabaseManager
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
                             QLabel, QGroupBox, QLineEdit, QFileDialog, QProgressBar, 
                             QCheckBox, QInputDialog)
from PyQt5.QtCore import pyqtSignal, Qt  # 💡 [버그픽스] Qt 임포트 추가!
from core.ProcessManager import ProcessManager

class ProductionTab(QWidget):
    sig_log = pyqtSignal(str, bool)

    def __init__(self, data_dir):
        super().__init__()
        self.data_dir = data_dir
        self.db = DatabaseManager()
        self.start_time = None
        self.final_stats = {'events': 0, 'speed': 0.0}
        
        self.prod_manager = ProcessManager()
        self.prod_manager.stat_signal.connect(self.update_progress)
        self.prod_manager.state_signal.connect(self.handle_state)
        self.initUI()

    def initUI(self):
        layout = QVBoxLayout(self)

        file_group = QGroupBox("1. Select Raw Data File (.dat)")
        file_layout = QHBoxLayout()
        self.input_file = QLineEdit()
        btn_browse = QPushButton("📂 Browse...")
        btn_browse.clicked.connect(self.browse_file)
        file_layout.addWidget(self.input_file); file_layout.addWidget(btn_browse)
        file_group.setLayout(file_layout); layout.addWidget(file_group)

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
        self.progress.setAlignment(Qt.AlignCenter) # 💡 여기서 Qt가 사용됩니다.
        self.progress.setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }")
        
        action_layout.addWidget(self.chk_wave); action_layout.addWidget(self.btn_batch); action_layout.addWidget(self.btn_inter); action_layout.addWidget(self.progress)
        action_group.setLayout(action_layout); layout.addWidget(action_group)

        self.inter_group = QGroupBox("3. Interactive Controls (-d 전용)")
        self.inter_group.setEnabled(False) 
        inter_layout = QHBoxLayout()
        btn_prev = QPushButton("⏮ Prev"); btn_prev.clicked.connect(lambda: self.prod_manager.write_stdin('p'))
        btn_next = QPushButton("⏭ Next"); btn_next.clicked.connect(lambda: self.prod_manager.write_stdin('n'))
        btn_jump = QPushButton("🔢 Jump"); btn_jump.clicked.connect(self.cmd_jump)
        btn_quit = QPushButton("⏹ Quit"); btn_quit.clicked.connect(lambda: self.prod_manager.write_stdin('q'))

        inter_layout.addWidget(btn_prev); inter_layout.addWidget(btn_next); inter_layout.addWidget(btn_jump); inter_layout.addWidget(btn_quit)
        self.inter_group.setLayout(inter_layout); layout.addWidget(self.inter_group)
        layout.addStretch()

    def browse_file(self):
        filename, _ = QFileDialog.getOpenFileName(self, "Select Raw File", self.data_dir, "Data Files (*.dat)")
        if filename: self.input_file.setText(filename)

    def run_batch(self):
        infile = self.input_file.text().strip()
        if not infile: return
        self.progress.setValue(0); self.start_time = datetime.now()
        self.final_stats = {'events': 0, 'speed': 0.0}
        bin_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../bin/production_500_mini"))
        args = [infile]
        if self.chk_wave.isChecked(): args.append("-w")
        self.prod_manager.start_process(bin_path, args)

    def run_interactive(self):
        infile = self.input_file.text().strip()
        if not infile: return
        self.start_time = None # 인터랙티브 모드는 DB 기록 생략
        bin_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../bin/production_500_mini"))
        self.prod_manager.start_process(bin_path, [infile, "-d"])

    def cmd_jump(self):
        num, ok = QInputDialog.getInt(self, "Jump to Event", "이동할 이벤트 번호:")
        if ok: self.prod_manager.write_stdin(f"j\n{num}")

    def update_progress(self, stats):
        if 'progress' in stats: self.progress.setValue(int(stats['progress']))
        if 'final_events' in stats: self.final_stats['events'] = stats['final_events']
        if 'final_speed' in stats: self.final_stats['speed'] = stats['final_speed']

    def handle_state(self, is_running):
        self.btn_batch.setEnabled(not is_running)
        self.btn_inter.setEnabled(not is_running)
        self.inter_group.setEnabled(is_running)

        if not is_running and self.start_time is not None:
            self.progress.setValue(100)
            elapsed = (datetime.now() - self.start_time).total_seconds()
            
            try: run_num = int(''.join(filter(str.isdigit, os.path.basename(self.input_file.text()))))
            except: run_num = 0
                
            self.db.insert_production_summary(
                run_num=run_num,
                elapsed=round(elapsed, 2),
                events=self.final_stats.get('events', 0),
                speed=self.final_stats.get('speed', 0.0),
                mode="Waveform" if self.chk_wave.isChecked() else "Fast Physics"
            )
            self.sig_log.emit("\033[1;32m[DB] Production Summary saved.\033[0m", False)
            self.start_time = None

    def force_abort(self):
        self.prod_manager.stop_process()
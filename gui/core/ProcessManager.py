import os
import re
import signal
from PySide6.QtCore import QObject, QProcess, QProcessEnvironment, Signal, QTimer
from core.Paths import get_project_root


class ProcessManager(QObject):
    log_signal = Signal(str, bool)
    state_signal = Signal(bool)
    stat_signal = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.process = None
        self._proc_gen = 0
        self.ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
        self._init_process()

    def _init_process(self):
        self._proc_gen += 1
        if self.process is not None:
            if self.process.state() != QProcess.NotRunning:
                self.process.kill()
                self.process.waitForFinished(500)
            self.process.deleteLater()

        self.process = QProcess(self)
        self.process.setProcessChannelMode(QProcess.MergedChannels)
        self.process.readyReadStandardOutput.connect(self.handle_stdout)
        self.process.stateChanged.connect(self.handle_state_change)

    def is_running(self) -> bool:
        return self.process is not None and self.process.state() == QProcess.Running

    def start_process(self, executable_path, arguments=None):
        if arguments is None:
            arguments = []

        if not os.path.exists(executable_path):
            self.log_signal.emit(
                f"<span style='color: #D32F2F;'><b>[ERROR] Executable not found: {executable_path} (run cmake & make first)</b></span>",
                True,
            )
            return

        self._init_process()

        root_dir = get_project_root()
        lib_dir = os.path.join(root_dir, "lib")
        env = QProcessEnvironment.systemEnvironment()
        existing_ld = env.value("LD_LIBRARY_PATH", "")
        env.insert("LD_LIBRARY_PATH", f"{lib_dir}:{existing_ld}" if existing_ld else lib_dir)
        self.process.setProcessEnvironment(env)
        self.process.setWorkingDirectory(root_dir)

        self.log_signal.emit(
            f"<span style='color: #2E7D32;'><b>[GUI] Launching {os.path.basename(executable_path)}...</b></span>",
            False,
        )
        self.process.start(executable_path, arguments)

    def stop_process(self):
        if self.is_running():
            target_gen = self._proc_gen
            self.log_signal.emit(
                "<span style='color: #F57C00;'><b>[GUI] Sending interrupt signal (SIGINT)...</b></span>",
                False,
            )
            try:
                os.kill(self.process.processId(), signal.SIGINT)
            except Exception:
                self.process.terminate()

            # Allow up to 8.0 s for post-stop DRAM aligned tail drain & USB3 flush,
            # guarded by generation token so a subsequent run is never killed by a stale timer!
            QTimer.singleShot(8000, lambda gen=target_gen: self.force_kill(gen))

    def force_kill(self, expected_gen=None):
        if expected_gen is not None and expected_gen != self._proc_gen:
            return
        if self.is_running():
            self.log_signal.emit(
                "<span style='color: #D32F2F; font-weight: bold;'>[GUI] Process did not exit within 8.0s. FORCING KILL!</span>",
                True,
            )
            self.process.kill()

    def write_stdin(self, text):
        if self.is_running():
            self.process.write((text + "\n").encode("utf-8"))

    def handle_stdout(self):
        if not self.process:
            return
        raw_text = bytes(self.process.readAllStandardOutput()).decode("utf-8", errors="ignore")

        for line in raw_text.split("\n"):
            for subline in line.split("\r"):
                if not subline.strip():
                    continue

                clean_line = self.ansi_escape.sub("", subline).strip()
                clean_line = clean_line.replace("[F[K", "").replace("[K", "")

                if "Real-time Monitor" in clean_line:
                    continue

                if "Loading Configuration:" in clean_line:
                    self.log_signal.emit(
                        "<span style='color: #388E3C;'>[INFO] Configuration file loaded successfully.</span>",
                        False,
                    )
                    continue

                if "Configuration backed up to:" in clean_line:
                    continue

                if "Events:" in clean_line and "Rate:" in clean_line:
                    stats = {}
                    m_ev = re.search(r"Events:\s*(\d+)", clean_line)
                    m_sz = re.search(r"Size:\s*([0-9.]+)\s*MB", clean_line)
                    m_rt = re.search(r"Rate:\s*([0-9.]+)\s*Hz", clean_line)
                    m_sp = re.search(r"Speed:\s*([0-9.]+)\s*MB/s", clean_line)
                    m_dq = re.search(r"DataQ:\s*(\d+)", clean_line)
                    m_pl = re.search(r"Pool:\s*(\d+)", clean_line)

                    if m_ev: stats["events"] = int(m_ev.group(1))
                    if m_sz: stats["size"] = m_sz.group(1)
                    if m_rt: stats["rate"] = m_rt.group(1)
                    if m_sp: stats["speed"] = m_sp.group(1)
                    if m_dq: stats["dataq"] = m_dq.group(1)
                    if m_pl: stats["pool"] = m_pl.group(1)

                    self.stat_signal.emit(stats)
                    continue

                if "Progress:" in clean_line:
                    stats = {}
                    m_prg = re.search(r"Progress:\s*([0-9.]+)\s*%", clean_line)
                    m_sp = re.search(r"Speed:\s*([0-9.]+)\s*MB/s", clean_line)
                    if m_prg: stats["progress"] = float(m_prg.group(1))
                    if m_sp: stats["final_speed"] = float(m_sp.group(1))
                    if stats:
                        self.stat_signal.emit(stats)
                    continue

                if "Total Events  :" in clean_line:
                    m = re.search(r"(\d+)", clean_line)
                    if m:
                        self.stat_signal.emit({"final_events": int(m.group(1))})

                if "Avg Speed     :" in clean_line:
                    m = re.search(r"([0-9.]+)\s*MB/s", clean_line)
                    if m:
                        self.stat_signal.emit({"final_speed": float(m.group(1))})

                html_line = subline.replace("\033[1;36m", "<span style='color: #0288D1;'>")
                html_line = html_line.replace("\033[1;32m", "<span style='color: #388E3C;'>")
                html_line = html_line.replace("\033[1;33m", "<span style='color: #F57C00;'>")
                html_line = html_line.replace("\033[1;31m", "<span style='color: #D32F2F;'>")
                html_line = html_line.replace("\033[1;35m", "<span style='color: #6A1B9A;'>")
                html_line = html_line.replace("\033[0m", "</span>")
                html_line = self.ansi_escape.sub("", html_line)
                self.log_signal.emit(html_line, "[ERROR]" in clean_line or "[FATAL]" in clean_line)

    def handle_state_change(self, state):
        if state == QProcess.NotRunning:
            self.state_signal.emit(False)
        elif state == QProcess.Running:
            self.state_signal.emit(True)

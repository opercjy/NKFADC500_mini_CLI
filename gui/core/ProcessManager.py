import os
import re
import signal 
from PySide6.QtCore import QObject, QProcess, Signal, QTimer

class ProcessManager(QObject):
    log_signal = Signal(str, bool)
    state_signal = Signal(bool)
    stat_signal = Signal(dict) 

    def __init__(self, parent=None):
        super().__init__(parent)
        self.process = None
        self.ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
        self._init_process()

    def _init_process(self):
        """
        💡 [핵심 버그픽스] 이전 프로세스의 찌꺼기(버퍼, 파이프)를 완전히 소각하고 
        메모리 누수 없이 깨끗한 QProcess를 새로 할당합니다.
        """
        if self.process is not None:
            self.process.kill()
            self.process.deleteLater() # 이전 객체 메모리에서 안전하게 해제
            
        self.process = QProcess()
        self.process.setProcessChannelMode(QProcess.MergedChannels)
        self.process.readyReadStandardOutput.connect(self.handle_stdout)
        self.process.stateChanged.connect(self.handle_state_change)

    def start_process(self, executable_path, arguments=[]):
        # 실행할 때마다 QProcess를 새로 고침하여 GUI 프리징(Freeze) 원천 차단
        self._init_process()
        
        self.log_signal.emit("<span style='color: #2E7D32;'><b>[GUI] Starting Data Acquisition Engine...</b></span>", False)
        
        # C++ 바이너리가 config/ 폴더를 정상적으로 찾을 수 있도록 작업 경로를 최상위로 고정
        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
        self.process.setWorkingDirectory(root_dir)
        self.process.start(executable_path, arguments)

    def stop_process(self):
        if self.process and self.process.state() == QProcess.Running:
            self.log_signal.emit("<span style='color: #F57C00;'><b>[GUI] Sending interrupt signal (Ctrl+C)...</b></span>", False)
            try:
                os.kill(self.process.processId(), signal.SIGINT)
            except Exception:
                pass
            
            QTimer.singleShot(1500, self.force_kill)

    def force_kill(self):
        if self.process and self.process.state() == QProcess.Running:
            self.log_signal.emit("<span style='color: #D32F2F; font-weight: bold;'>[GUI] Hardware deadlock detected. FORCING KILL!</span>", True)
            self.process.kill() 

    def write_stdin(self, text):
        if self.process and self.process.state() == QProcess.Running:
            self.process.write((text + "\n").encode('utf-8'))

    def handle_stdout(self):
        if not self.process: return
        raw_text = bytes(self.process.readAllStandardOutput()).decode('utf-8', errors='ignore')
        
        for line in raw_text.split('\n'):
            for subline in line.split('\r'):
                if not subline.strip(): continue
                
                clean_line = self.ansi_escape.sub('', subline).strip()
                clean_line = clean_line.replace('[F[K', '').replace('[K', '')
                
                if "Real-time Monitor" in clean_line: continue
                
                if "Loading Configuration:" in clean_line:
                    self.log_signal.emit("<span style='color: #388E3C;'>[INFO] Configuration file loaded successfully.</span>", False)
                    continue  
                    
                if "Configuration backed up to:" in clean_line:
                    continue  
                
                if "Events:" in clean_line and "Rate:" in clean_line:
                    stats = {}
                    m_ev = re.search(r'Events:\s*(\d+)', clean_line)
                    m_sz = re.search(r'Size:\s*([0-9.]+)\s*MB', clean_line)
                    m_rt = re.search(r'Rate:\s*([0-9.]+)\s*Hz', clean_line)
                    m_sp = re.search(r'Speed:\s*([0-9.]+)\s*MB/s', clean_line)
                    m_dq = re.search(r'DataQ:\s*(\d+)', clean_line)
                    m_pl = re.search(r'Pool:\s*(\d+)', clean_line)
                    
                    if m_ev: stats['events'] = int(m_ev.group(1))
                    if m_sz: stats['size'] = m_sz.group(1)
                    if m_rt: stats['rate'] = m_rt.group(1)
                    if m_sp: stats['speed'] = m_sp.group(1)
                    if m_dq: stats['dataq'] = m_dq.group(1)
                    if m_pl: stats['pool'] = m_pl.group(1)
                    
                    self.stat_signal.emit(stats) 
                    continue 

                if "Progress:" in clean_line:
                    m_prg = re.search(r'Progress:\s*([0-9.]+)\s*%', clean_line)
                    if m_prg: self.stat_signal.emit({'progress': float(m_prg.group(1))})
                    continue 
                
                if "Total Events  :" in clean_line:
                    self.stat_signal.emit({'final_events': int(re.search(r'\d+', clean_line).group())})

                html_line = subline.replace('\033[1;36m', "<span style='color: #0288D1;'>")
                html_line = html_line.replace('\033[1;32m', "<span style='color: #388E3C;'>")
                html_line = html_line.replace('\033[1;33m', "<span style='color: #F57C00;'>")
                html_line = html_line.replace('\033[1;31m', "<span style='color: #D32F2F;'>")
                html_line = html_line.replace('\033[0m', "</span>")
                self.ansi_escape.sub('', html_line)
                self.log_signal.emit(html_line, False)

    def handle_state_change(self, state):
        if state == QProcess.NotRunning:
            self.state_signal.emit(False)
        elif state == QProcess.Running:
            self.state_signal.emit(True)

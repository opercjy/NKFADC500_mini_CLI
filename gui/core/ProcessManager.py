import os
import re
import signal 
from PyQt5.QtCore import QObject, QProcess, pyqtSignal, QTimer

class ProcessManager(QObject):
    log_signal = pyqtSignal(str, bool)
    state_signal = pyqtSignal(bool)
    stat_signal = pyqtSignal(dict) 

    def __init__(self, parent=None):
        super().__init__(parent)
        self.process = QProcess()
        self.process.setProcessChannelMode(QProcess.MergedChannels)
        self.process.readyReadStandardOutput.connect(self.handle_stdout)
        self.process.stateChanged.connect(self.handle_state_change)
        
        self.ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

    def start_process(self, executable_path, arguments=[]):
        if self.process.state() == QProcess.NotRunning:
            # 💡 [GUI 로그 히든 처리] 길고 지저분한 실행 명령어 대신 심플한 메시지 출력
            self.log_signal.emit("<span style='color: #2E7D32;'><b>[GUI] Starting Data Acquisition Engine...</b></span>", False)
            self.process.start(executable_path, arguments)

    def stop_process(self):
        if self.process.state() == QProcess.Running:
            self.log_signal.emit("<span style='color: #F57C00;'><b>[GUI] Sending interrupt signal (Ctrl+C)...</b></span>", False)
            try:
                os.kill(self.process.processId(), signal.SIGINT)
            except Exception:
                pass
            
            QTimer.singleShot(1500, self.force_kill)

    def force_kill(self):
        if self.process.state() == QProcess.Running:
            self.log_signal.emit("<span style='color: #D32F2F; font-weight: bold;'>[GUI] Hardware deadlock detected. FORCING KILL!</span>", True)
            self.process.kill() 

    def write_stdin(self, text):
        if self.process.state() == QProcess.Running:
            self.process.write((text + "\n").encode('utf-8'))

    def handle_stdout(self):
        raw_text = bytes(self.process.readAllStandardOutput()).decode('utf-8', errors='ignore')
        
        for line in raw_text.split('\n'):
            for subline in line.split('\r'):
                if not subline.strip(): continue
                
                clean_line = self.ansi_escape.sub('', subline).strip()
                clean_line = clean_line.replace('[F[K', '').replace('[K', '')
                
                if "Real-time Monitor" in clean_line: continue
                
                # ====================================================================
                # 💡 [백엔드 로그 필터링] C++에서 올라오는 지저분한 경로를 가로채서 예쁘게 바꿈
                # ====================================================================
                if "Loading Configuration:" in clean_line:
                    self.log_signal.emit("<span style='color: #388E3C;'>[INFO] Configuration file loaded successfully.</span>", False)
                    continue  # 원본 로그는 무시
                    
                if "Configuration backed up to:" in clean_line:
                    continue  # 백업 안내 로그는 아예 화면에 안 띄우고 숨김 처리
                # ====================================================================
                
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
                if "Avg Speed     :" in clean_line:
                    self.stat_signal.emit({'final_speed': float(re.search(r'([0-9.]+)', clean_line).group(1))})

                # 일반 로그는 ANSI 컬러를 HTML로 변환해서 출력
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
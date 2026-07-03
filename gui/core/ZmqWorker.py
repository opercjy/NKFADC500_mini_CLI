import zmq
import numpy as np
from PySide6.QtCore import QThread, Signal

class ZmqWorker(QThread):
    # 💡 [핵심 버그픽스] PySide6에서 Numpy 배열 통과를 위한 object 자료형 캐스팅
    waveform_received = Signal(int, object)

    def __init__(self, port=5555, parent=None):
        super().__init__(parent)
        self.port = port
        self.is_running = True
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        
        self.socket.setsockopt(zmq.CONFLATE, 1)
        self.socket.connect(f"tcp://127.0.0.1:{self.port}")
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

    def run(self):
        while self.is_running:
            try:
                if self.socket.poll(100): 
                    msg = self.socket.recv()
                    
                    if len(msg) > 4:
                        ch_id = int.from_bytes(msg[:4], byteorder='little')
                        waveform = np.frombuffer(msg[4:], dtype=np.uint16)
                        self.waveform_received.emit(ch_id, waveform)
            except zmq.ZMQError:
                pass

    def stop(self):
        self.is_running = False
        self.wait()
        self.socket.close()
        self.context.term()
import os
import glob
import numpy as np
import pyqtgraph as pg
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
                               QLabel, QCheckBox, QRadioButton, QGroupBox, QSplitter,
                               QSpinBox, QDoubleSpinBox)
from PySide6.QtCore import Qt, QTimer, Slot

pg.setConfigOption('background', '#ECEFF1')
pg.setConfigOption('foreground', '#263238')

class OnlineMonitorTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.current_file = "" 
        self.file_pos = 0 
        
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.read_binary_chunk)
        
        self.latest_waveforms = {0: [], 1: [], 2: [], 3: []}
        self.hist_data = {0: [], 1: [], 2: [], 3: []} 
        
        self.curves_wave = {}
        self.curves_spec = {}
        
        self.colors = [(220, 30, 30), (30, 160, 30), (30, 100, 220), (220, 120, 0)]
        self._init_ui()

    def _init_ui(self):
        layout = QVBoxLayout(self)

        # 1. 제어 버튼 패널
        ctrl_layout = QHBoxLayout()
        self.btn_start = QPushButton("▶ Start Live Monitor")
        self.btn_start.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 5px;")
        
        self.btn_stop = QPushButton("⏹ Stop Monitor")
        self.btn_stop.setStyleSheet("background-color: #F44336; color: white; font-weight: bold; padding: 5px;")
        self.btn_stop.setEnabled(False)
        
        # 💡 [신규 추가] 누적 데이터 및 파형 초기화 버튼
        self.btn_clear = QPushButton("🔄 Clear Plots")
        self.btn_clear.setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 5px;")
        
        self.lbl_status = QLabel("Status: Stopped")
        self.lbl_status.setStyleSheet("color: red; font-weight: bold;")
        
        self.btn_start.clicked.connect(self.start_monitoring)
        self.btn_stop.clicked.connect(self.stop_monitoring)
        self.btn_clear.clicked.connect(self.clear_plots)

        ctrl_layout.addWidget(self.btn_start)
        ctrl_layout.addWidget(self.btn_stop)
        ctrl_layout.addWidget(self.btn_clear)
        ctrl_layout.addWidget(self.lbl_status)
        ctrl_layout.addStretch()
        layout.addLayout(ctrl_layout)

        # 2. 실시간 분석 옵션 패널
        opt_layout = QHBoxLayout()
        
        ch_group = QGroupBox("Channel Visibility")
        ch_h_layout = QHBoxLayout()
        self.ch_checkboxes = {}
        for i in range(4):
            cb = QCheckBox(f"Ch {i}")
            cb.setChecked(True)
            cb.setStyleSheet(f"color: rgb{self.colors[i]}; font-weight: bold;")
            cb.stateChanged.connect(self.update_visibility)
            self.ch_checkboxes[i] = cb
            ch_h_layout.addWidget(cb)
        ch_group.setLayout(ch_h_layout)

        mode_group = QGroupBox("Analytics Mode")
        mode_h_layout = QHBoxLayout()
        self.radio_amp = QRadioButton("Pulse Amplitude (Peak)")
        self.radio_charge = QRadioButton("Pulse Integral (Charge)")
        self.radio_amp.setChecked(True)
        self.radio_amp.toggled.connect(self.toggle_mode)
        mode_h_layout.addWidget(self.radio_amp)
        mode_h_layout.addWidget(self.radio_charge)
        mode_group.setLayout(mode_h_layout)

        param_group = QGroupBox("Display Parameters")
        param_layout = QHBoxLayout()
        
        param_layout.addWidget(QLabel("Update Interval (s):"))
        self.spin_interval = QDoubleSpinBox()
        self.spin_interval.setRange(0.01, 2.0)
        self.spin_interval.setSingleStep(0.05)
        self.spin_interval.setValue(0.05) 
        
        param_layout.addWidget(QLabel("Accumulate Events:"))
        self.spin_accum = QSpinBox()
        self.spin_accum.setRange(100, 100000)
        self.spin_accum.setSingleStep(1000)
        self.spin_accum.setValue(5000) 
        
        param_layout.addWidget(self.spin_interval)
        param_layout.addWidget(self.spin_accum)
        param_group.setLayout(param_layout)

        opt_layout.addWidget(ch_group)
        opt_layout.addWidget(mode_group)
        opt_layout.addWidget(param_group)
        opt_layout.addStretch()
        layout.addLayout(opt_layout)

        # 3. 1:2 비율 그리드 뷰어 세팅
        self.splitter = QSplitter(Qt.Horizontal)
        
        self.plot_wave = pg.PlotWidget(title="Live Waveform (Time Domain)")
        self.plot_wave.setLabel('left', 'Voltage Drop', units='ADC')
        self.plot_wave.setLabel('bottom', 'Time', units='ns (2.0ns/Sample)')
        self.plot_wave.showGrid(x=True, y=True, alpha=0.3)
        self.plot_wave.enableAutoRange(axis='y') 
        
        self.plot_spec = pg.PlotWidget(title="Pulse Spectrum (Histogram)")
        self.plot_spec.setLabel('left', 'Counts')
        self.plot_spec.setLabel('bottom', 'Amplitude (ADC)')
        self.plot_spec.showGrid(x=True, y=True, alpha=0.3)
        self.plot_spec.addLegend()

        self.splitter.addWidget(self.plot_wave)
        self.splitter.addWidget(self.plot_spec)
        self.splitter.setStretchFactor(0, 1)
        self.splitter.setStretchFactor(1, 1)
        layout.addWidget(self.splitter)

        for i in range(4):
            color = self.colors[i]
            self.curves_wave[i] = self.plot_wave.plot(pen=pg.mkPen(color, width=2), name=f"Ch {i}")
            self.curves_spec[i] = pg.PlotDataItem(stepMode=True, fillLevel=0, 
                                         brush=pg.mkBrush(color[0], color[1], color[2], 80), 
                                         pen=pg.mkPen(color, width=2), name=f"Ch {i}")
            self.plot_spec.addItem(self.curves_spec[i])

    @Slot()
    def toggle_mode(self):
        self.clear_plots()
        if self.radio_amp.isChecked():
            self.plot_spec.setLabel('bottom', 'Amplitude (ADC)')
        else:
            self.plot_spec.setLabel('bottom', 'Charge (ADC*ns)')

    @Slot()
    def update_visibility(self):
        for i, cb in self.ch_checkboxes.items():
            is_visible = cb.isChecked()
            self.curves_wave[i].setVisible(is_visible)
            self.curves_spec[i].setVisible(is_visible)

    # 💡 [핵심] Clear 버튼 연동 로직: 즉시 모든 메모리를 비우고 그래프를 지움
    @Slot()
    def clear_plots(self):
        for i in range(4):
            self.hist_data[i].clear()
            self.latest_waveforms[i] = []
            if i in self.curves_wave:
                self.curves_wave[i].setData([], [])
            if i in self.curves_spec:
                self.curves_spec[i].setData([], [])
        
        if not self.timer.isActive():
            self.lbl_status.setText("Status: Plots Cleared.")
            self.lbl_status.setStyleSheet("color: blue; font-weight: bold;")

    @Slot()
    def start_monitoring(self):
        if not self.current_file or not os.path.exists(self.current_file):
            data_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../data"))
            dat_files = glob.glob(os.path.join(data_dir, "*.dat"))
            if dat_files:
                self.current_file = max(dat_files, key=os.path.getmtime)
            else:
                self.lbl_status.setText("Status: Error - No .dat file found in data directory!")
                self.lbl_status.setStyleSheet("color: red; font-weight: bold;")
                return

        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)
        self.lbl_status.setText(f"Status: Direct Parsing {os.path.basename(self.current_file)}...")
        self.lbl_status.setStyleSheet("color: green; font-weight: bold;")
        
        self.clear_plots()
        self.file_pos = 0 
        
        interval_ms = int(self.spin_interval.value() * 1000)
        self.timer.start(interval_ms)

    @Slot()
    def stop_monitoring(self):
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        self.lbl_status.setText("Status: Stopped")
        self.lbl_status.setStyleSheet("color: red; font-weight: bold;")
        self.timer.stop()

    @Slot()
    def read_binary_chunk(self):
        if not os.path.exists(self.current_file): return

        try:
            with open(self.current_file, 'rb') as f:
                f.seek(self.file_pos)
                events_parsed = 0
                max_events_per_tick = 3000

                while events_parsed < max_events_per_tick:
                    header = f.read(128)
                    if len(header) < 128:
                        break
                    
                    data_length = header[0] + (header[4] << 8) + (header[8] << 16) + (header[12] << 24)
                    
                    if data_length <= 32 or data_length > 100000000:
                        break

                    record_length = (data_length - 32) // 2
                    payload_bytes = record_length * 8
                    payload = f.read(payload_bytes)
                    
                    if len(payload) < payload_bytes:
                        break

                    self.file_pos = f.tell() 
                    events_parsed += 1

                    raw = np.frombuffer(payload, dtype=np.uint8).reshape(-1, 8)
                    ch0 = (raw[:, 0].astype(np.uint16) | (raw[:, 4].astype(np.uint16) << 8)) & 0x0FFF
                    ch1 = (raw[:, 1].astype(np.uint16) | (raw[:, 5].astype(np.uint16) << 8)) & 0x0FFF
                    ch2 = (raw[:, 2].astype(np.uint16) | (raw[:, 6].astype(np.uint16) << 8)) & 0x0FFF
                    ch3 = (raw[:, 3].astype(np.uint16) | (raw[:, 7].astype(np.uint16) << 8)) & 0x0FFF
                    
                    waveforms = [ch0, ch1, ch2, ch3]

                    if events_parsed == 1 or events_parsed == max_events_per_tick:
                        self.latest_waveforms = waveforms

                    max_accum = self.spin_accum.value()
                    for ch_id in range(4):
                        wf = waveforms[ch_id]
                        n_ped = min(20, len(wf))
                        baseline = np.mean(wf[:n_ped]) if n_ped > 0 else 0
                        inverted = baseline - wf

                        val = np.max(inverted) if self.radio_amp.isChecked() else np.sum(inverted[inverted > 0])
                        self.hist_data[ch_id].append(val)
                        
                        if len(self.hist_data[ch_id]) > max_accum:
                            self.hist_data[ch_id].pop(0)

                if events_parsed > 0:
                    self.update_plots()

        except Exception as e:
            pass

    def update_plots(self):
        for ch_id in range(4):
            if not self.ch_checkboxes[ch_id].isChecked(): continue

            wf = self.latest_waveforms[ch_id]
            if len(wf) > 0:
                n_ped = min(20, len(wf))
                baseline = np.mean(wf[:n_ped]) if n_ped > 0 else 0
                inverted = baseline - wf 
                
                x_data = np.arange(len(wf)) * 2.0 
                self.curves_wave[ch_id].setData(x_data, inverted)

            data_arr = self.hist_data[ch_id]
            if len(data_arr) > 2:
                y, x = np.histogram(data_arr, bins=100)
                self.curves_spec[ch_id].setData(x, y)
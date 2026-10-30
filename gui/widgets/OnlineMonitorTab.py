from collections import deque
import glob
import os
import numpy as np
import pyqtgraph as pg
from PySide6.QtCore import QTimer, Qt, Slot
from PySide6.QtWidgets import (
    QCheckBox,
    QDoubleSpinBox,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QRadioButton,
    QSpinBox,
    QSplitter,
    QVBoxLayout,
    QWidget,
)
from core.Paths import get_default_data_dir

pg.setConfigOption("background", "#ECEFF1")
pg.setConfigOption("foreground", "#263238")


class OnlineMonitorTab(QWidget):
    def __init__(self, data_dir=None, parent=None):
        super().__init__(parent)
        self.data_dir = os.path.abspath(data_dir) if data_dir else get_default_data_dir()
        self.current_file = ""
        self.file_pos = 0

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.read_binary_chunk)

        self.latest_waveforms = {0: np.array([]), 1: np.array([]), 2: np.array([]), 3: np.array([])}
        self.hist_data = {i: deque(maxlen=5000) for i in range(4)}

        self.curves_wave = {}
        self.curves_spec = {}

        self.colors = [(220, 30, 30), (30, 160, 30), (30, 100, 220), (220, 120, 0)]
        self._init_ui()

    def _init_ui(self):
        layout = QVBoxLayout(self)

        ctrl_layout = QHBoxLayout()
        self.btn_start = QPushButton("▶ Start Live Monitor")
        self.btn_start.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 5px;")

        self.btn_stop = QPushButton("⏹ Stop Monitor")
        self.btn_stop.setStyleSheet("background-color: #F44336; color: white; font-weight: bold; padding: 5px;")
        self.btn_stop.setEnabled(False)

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
        self.spin_interval.setRange(0.02, 2.0)
        self.spin_interval.setSingleStep(0.05)
        self.spin_interval.setValue(0.05)
        self.spin_interval.valueChanged.connect(self._on_interval_changed)

        param_layout.addWidget(QLabel("Accumulate Events:"))
        self.spin_accum = QSpinBox()
        self.spin_accum.setRange(100, 100000)
        self.spin_accum.setSingleStep(1000)
        self.spin_accum.setValue(5000)
        self.spin_accum.valueChanged.connect(self._on_accum_changed)

        param_layout.addWidget(self.spin_interval)
        param_layout.addWidget(self.spin_accum)
        param_group.setLayout(param_layout)

        opt_layout.addWidget(ch_group)
        opt_layout.addWidget(mode_group)
        opt_layout.addWidget(param_group)
        opt_layout.addStretch()
        layout.addLayout(opt_layout)

        self.splitter = QSplitter(Qt.Horizontal)

        self.plot_wave = pg.PlotWidget(title="Live Waveform (Time Domain)")
        self.plot_wave.setLabel("left", "Voltage Drop", units="ADC")
        self.plot_wave.setLabel("bottom", "Time", units="ns (2.0 ns/Sample)")
        self.plot_wave.showGrid(x=True, y=True, alpha=0.3)
        self.plot_wave.enableAutoRange(axis="y")

        self.plot_spec = pg.PlotWidget(title="Pulse Spectrum (Histogram)")
        self.plot_spec.setLabel("left", "Counts")
        self.plot_spec.setLabel("bottom", "Amplitude (ADC)")
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
            self.curves_spec[i] = pg.PlotDataItem(
                stepMode="center",
                fillLevel=0,
                brush=pg.mkBrush(color[0], color[1], color[2], 80),
                pen=pg.mkPen(color, width=2),
                name=f"Ch {i}",
            )
            self.plot_spec.addItem(self.curves_spec[i])

    @Slot(str)
    def set_target_file(self, filepath: str):
        if filepath and filepath != self.current_file:
            self.current_file = filepath
            self.file_pos = 0
            if self.timer.isActive():
                self.clear_plots()
                self.lbl_status.setText(f"Status: Direct Parsing {os.path.basename(self.current_file)}...")

    @Slot()
    def _on_interval_changed(self):
        if self.timer.isActive():
            self.timer.setInterval(max(20, int(self.spin_interval.value() * 1000)))

    @Slot()
    def _on_accum_changed(self):
        new_max = self.spin_accum.value()
        for ch_id in range(4):
            self.hist_data[ch_id] = deque(self.hist_data[ch_id], maxlen=new_max)

    @Slot()
    def toggle_mode(self):
        self.clear_plots()
        if self.radio_amp.isChecked():
            self.plot_spec.setLabel("bottom", "Amplitude (ADC)")
        else:
            self.plot_spec.setLabel("bottom", "Charge (ADC*ns)")

    @Slot()
    def update_visibility(self):
        for i, cb in self.ch_checkboxes.items():
            is_visible = cb.isChecked()
            self.curves_wave[i].setVisible(is_visible)
            self.curves_spec[i].setVisible(is_visible)

    @Slot()
    def clear_plots(self):
        max_accum = self.spin_accum.value()
        for i in range(4):
            self.hist_data[i] = deque(maxlen=max_accum)
            self.latest_waveforms[i] = np.array([])
            if i in self.curves_wave:
                self.curves_wave[i].setData([], [])
            if i in self.curves_spec:
                self.curves_spec[i].setData([], [])

        if not self.timer.isActive():
            self.lbl_status.setText("Status: Plots Cleared.")
            self.lbl_status.setStyleSheet("color: blue; font-weight: bold;")

    @Slot()
    def start_monitoring(self):
        dat_files = glob.glob(os.path.join(self.data_dir, "*.dat"))
        if not self.current_file or not os.path.exists(self.current_file):
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

        interval_ms = max(20, int(self.spin_interval.value() * 1000))
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
        if not self.current_file or not os.path.exists(self.current_file):
            return

        try:
            file_size = os.path.getsize(self.current_file)
            if file_size < self.file_pos:
                # Truncated / overwritten file on new run
                self.file_pos = 0
                self.clear_plots()

            if file_size - self.file_pos < 128:
                return

            with open(self.current_file, "rb") as f:
                f.seek(self.file_pos)

                # Peek first 128-byte header to determine fixed event packet geometry
                header = f.read(128)
                if len(header) < 128:
                    return

                data_length = header[0] | (header[4] << 8) | (header[8] << 16) | (header[12] << 24)
                if data_length <= 32 or data_length > 100000000 or ((data_length - 32) % 2 != 0):
                    return

                record_length = (data_length - 32) // 2
                event_bytes = 128 + record_length * 8
                avail_events = min(3000, (file_size - self.file_pos) // event_bytes)
                if avail_events <= 0:
                    return

                # Vectorized multi-event read & NumPy unpacking (50x faster than Python per-event loop!)
                f.seek(self.file_pos)
                chunk = f.read(avail_events * event_bytes)
                n_ev = len(chunk) // event_bytes
                if n_ev <= 0:
                    return

                self.file_pos += n_ev * event_bytes

                ev_matrix = np.frombuffer(chunk[: n_ev * event_bytes], dtype=np.uint8).reshape(n_ev, event_bytes)
                payloads = ev_matrix[:, 128:].reshape(n_ev, record_length, 8)
                lo = payloads[:, :, :4].astype(np.uint16)
                hi = payloads[:, :, 4:].astype(np.uint16) << 8
                waves = (lo | hi) & 0x0FFF  # shape: (n_ev, record_length, 4)

                for ch_id in range(4):
                    self.latest_waveforms[ch_id] = waves[-1, :, ch_id]

                n_ped = min(20, record_length)
                baselines = np.mean(waves[:, :n_ped, :], axis=1, keepdims=True)
                inverted = baselines - waves  # shape: (n_ev, record_length, 4)

                if self.radio_amp.isChecked():
                    metrics = np.max(inverted, axis=1)  # shape: (n_ev, 4)
                else:
                    # Multiply ADC*sample sum by 2.0 ns/sample to match axis label "Charge (ADC*ns)"
                    metrics = np.sum(np.maximum(inverted, 0.0), axis=1) * 2.0

                for ch_id in range(4):
                    self.hist_data[ch_id].extend(metrics[:, ch_id].tolist())

                self.update_plots()

        except Exception:
            pass

    def update_plots(self):
        for ch_id in range(4):
            if not self.ch_checkboxes[ch_id].isChecked():
                continue

            wf = self.latest_waveforms[ch_id]
            if len(wf) > 0:
                n_ped = min(20, len(wf))
                baseline = float(np.mean(wf[:n_ped])) if n_ped > 0 else 0.0
                inverted = baseline - wf.astype(np.float64)
                x_data = np.arange(len(wf), dtype=np.float64) * 2.0
                self.curves_wave[ch_id].setData(x_data, inverted)

            data_arr = self.hist_data[ch_id]
            if len(data_arr) > 2:
                y, x_edges = np.histogram(np.fromiter(data_arr, dtype=np.float64, count=len(data_arr)), bins=100)
                self.curves_spec[ch_id].setData(x_edges, y)

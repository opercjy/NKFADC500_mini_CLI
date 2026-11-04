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
from core.Paths import get_default_config_dir, get_default_data_dir

pg.setConfigOption("background", "#ECEFF1")
pg.setConfigOption("foreground", "#263238")


def _normalize_sampling_divider(dsr: int) -> int:
    valid = (1, 2, 4, 8)
    return min(valid, key=lambda v: abs(v - dsr))


def _compute_baseline_samples(delay_ns: float, dt_ns: float, record_length: int) -> int:
    if record_length <= 0:
        return 0
    dt = dt_ns if dt_ns > 0.0 else 2.0
    if delay_ns <= 0.0:
        return min(8, record_length)
    pre_pts = int(delay_ns / dt)
    if pre_pts <= 4:
        return min(max(1, pre_pts), record_length)
    safe_win = max(4, int(pre_pts * 0.45))
    return max(1, min(safe_win, min(pre_pts - 2, record_length)))


class OnlineMonitorTab(QWidget):
    def __init__(self, data_dir=None, parent=None):
        super().__init__(parent)
        self.data_dir = os.path.abspath(data_dir) if data_dir else get_default_data_dir()
        self.current_file = ""
        self.file_pos = 0

        # Stream decoding parameters auto-synced from companion <run>.cfg / settings.cfg
        self.dt_ns = 2.0
        self.delay_ns = np.array([400.0, 400.0, 400.0, 400.0], dtype=np.float64)
        self.polarity = np.array([0, 0, 0, 0], dtype=np.int32)  # 0: negative pulse, 1: positive pulse

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.read_binary_chunk)

        self.latest_waveforms = {0: np.array([]), 1: np.array([]), 2: np.array([]), 3: np.array([])}
        self.hist_data = {i: deque(maxlen=5000) for i in range(4)}

        self.curves_wave = {}
        self.curves_spec = {}

        # Okabe-Ito colorblind-safe 4-channel palette:
        # Ch0: #0072B2 (Blue), Ch1: #D55E00 (Vermilion), Ch2: #009E73 (Bluish Green), Ch3: #CC79A7 (Reddish Purple)
        self.colors = [(0, 114, 178), (213, 94, 0), (0, 158, 115), (204, 121, 167)]
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
        self.plot_wave.setLabel("left", "Pulse Signal", units="ADC")
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

    def _load_companion_config(self, dat_filepath: str):
        dt_ns = 2.0
        delay_ns = np.array([400.0, 400.0, 400.0, 400.0], dtype=np.float64)
        polarity = np.array([0, 0, 0, 0], dtype=np.int32)

        candidates = []
        if dat_filepath:
            stem, _ = os.path.splitext(dat_filepath)
            candidates.append(stem + ".cfg")
        candidates.append(os.path.join(get_default_config_dir(), "settings.cfg"))

        cfg_path = next((p for p in candidates if p and os.path.exists(p)), "")
        if cfg_path:
            try:
                with open(cfg_path, "r", encoding="utf-8", errors="ignore") as cf:
                    for raw_line in cf:
                        clean = raw_line.split("#", 1)[0].strip()
                        if not clean:
                            continue
                        parts = clean.split()
                        key, vals = parts[0], parts[1:]
                        if not vals:
                            continue
                        if key == "SAMPLING_RATE":
                            dsr = _normalize_sampling_divider(int(vals[0], 0))
                            dt_ns = 2.0 * float(dsr)
                        elif key == "DLY":
                            fvals = [max(0.0, float(v)) for v in vals[:4]]
                            if len(fvals) == 1:
                                delay_ns[:] = fvals[0]
                            else:
                                for idx, fv in enumerate(fvals):
                                    delay_ns[idx] = fv
                        elif key == "POL":
                            ivals = [max(0, min(1, int(v, 0))) for v in vals[:4]]
                            if len(ivals) == 1:
                                polarity[:] = ivals[0]
                            else:
                                for idx, iv in enumerate(ivals):
                                    polarity[idx] = iv
            except Exception:
                pass

        self.dt_ns = dt_ns
        self.delay_ns = delay_ns
        self.polarity = polarity
        self.plot_wave.setLabel("bottom", "Time", units=f"ns ({self.dt_ns:.1f} ns/Sample)")

    @Slot(str)
    def set_target_file(self, filepath: str):
        if filepath and filepath != self.current_file:
            self.current_file = filepath
            self.file_pos = 0
            self._load_companion_config(self.current_file)
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

        self._load_companion_config(self.current_file)
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
                self._load_companion_config(self.current_file)
                self.clear_plots()

            if file_size - self.file_pos < 128:
                return

            with open(self.current_file, "rb") as f:
                if self.file_pos == 0:
                    self._load_companion_config(self.current_file)
                f.seek(self.file_pos)

                # Peek first 128-byte header to determine fixed event packet geometry
                header = f.read(128)
                if len(header) < 128:
                    return

                data_length = header[0] | (header[4] << 8) | (header[8] << 16) | (header[12] << 24)
                ch_tags = (header[64], header[65], header[66], header[67])
                valid_geom = (32 <= data_length <= 1048576) and ((data_length - 32) % 2 == 0)
                valid_tags = ch_tags in ((1, 2, 3, 4), (0, 0, 0, 0))
                if not (valid_geom and valid_tags):
                    return

                record_length = (data_length - 32) // 2
                event_bytes = 128 + record_length * 8
                avail_events = min(3000, (file_size - self.file_pos) // event_bytes)
                if avail_events <= 0:
                    return

                if record_length == 0:
                    # Zero-suppressed header-only packets (data_length == 32)
                    self.file_pos += avail_events * 128
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
                waves = ((lo | hi) & 0x0FFF).astype(np.float64)  # shape: (n_ev, record_length, 4)

                for ch_id in range(4):
                    self.latest_waveforms[ch_id] = waves[-1, :, ch_id]

                n_ped = _compute_baseline_samples(float(np.min(self.delay_ns)), self.dt_ns, record_length)
                baselines = np.mean(waves[:, :n_ped, :], axis=1, keepdims=True)
                pol_sign = np.where(self.polarity == 0, 1.0, -1.0).reshape(1, 1, 4)
                signals = (baselines - waves) * pol_sign  # positive-going for both POL=0 and POL=1

                if self.radio_amp.isChecked():
                    metrics = np.max(signals, axis=1)  # shape: (n_ev, 4)
                else:
                    metrics = np.sum(np.maximum(signals, 0.0), axis=1) * self.dt_ns

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
                n_ped = _compute_baseline_samples(float(self.delay_ns[ch_id]), self.dt_ns, len(wf))
                baseline = float(np.mean(wf[:n_ped])) if n_ped > 0 else 0.0
                sig = (baseline - wf) if self.polarity[ch_id] == 0 else (wf - baseline)
                x_data = np.arange(len(wf), dtype=np.float64) * self.dt_ns
                self.curves_wave[ch_id].setData(x_data, sig)

            data_arr = self.hist_data[ch_id]
            if len(data_arr) > 2:
                y, x_edges = np.histogram(np.fromiter(data_arr, dtype=np.float64, count=len(data_arr)), bins=100)
                self.curves_spec[ch_id].setData(x_edges, y)

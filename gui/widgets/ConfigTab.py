import os
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
                             QPlainTextEdit, QMessageBox, QLineEdit, QLabel, QFileDialog)
from PyQt5.QtGui import QFont

class ConfigTab(QWidget):
    def __init__(self):
        super().__init__()
        self.cfg_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../config/settings.cfg"))
        self.initUI()
        self.load_config()

    def initUI(self):
        layout = QVBoxLayout(self)

        # 파일 경로 탐색기
        file_layout = QHBoxLayout()
        self.path_input = QLineEdit(self.cfg_path)
        self.path_input.setStyleSheet("background-color: white; color: black; padding: 5px;")
        
        btn_browse = QPushButton("📂 Browse...")
        btn_browse.clicked.connect(self.browse_file)
        
        btn_load = QPushButton("📥 Load")
        btn_load.clicked.connect(self.load_config)
        
        file_layout.addWidget(QLabel("Config File:"))
        file_layout.addWidget(self.path_input)
        file_layout.addWidget(btn_browse)
        file_layout.addWidget(btn_load)
        layout.addLayout(file_layout)

        # 💡 눈이 편안한 밝은 테마의 에디터
        self.editor = QPlainTextEdit()
        self.editor.setFont(QFont("Consolas", 12))
        self.editor.setStyleSheet("background-color: #FFFFFF; color: #333333; border: 1px solid #CDBA96; padding: 10px;")
        layout.addWidget(self.editor)

        # 저장 버튼
        self.btn_save = QPushButton("💾 Save Configuration")
        self.btn_save.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 12px; font-size: 14px;")
        self.btn_save.clicked.connect(self.save_config)
        layout.addWidget(self.btn_save)

    def browse_file(self):
        filename, _ = QFileDialog.getOpenFileName(self, "Select Config File", os.path.dirname(self.cfg_path), "Config Files (*.cfg);;All Files (*)")
        if filename:
            self.path_input.setText(filename)
            self.load_config()

    def load_config(self):
        self.cfg_path = self.path_input.text()
        if os.path.exists(self.cfg_path):
            with open(self.cfg_path, 'r', encoding='utf-8') as f:
                self.editor.setPlainText(f.read())
        else:
            self.editor.setPlainText(f"# Cannot find file:\n# {self.cfg_path}")

    def save_config(self):
        self.cfg_path = self.path_input.text()
        try:
            with open(self.cfg_path, 'w', encoding='utf-8') as f:
                f.write(self.editor.toPlainText())
            QMessageBox.information(self, "Success", "Configuration saved successfully!")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save config:\n{e}")
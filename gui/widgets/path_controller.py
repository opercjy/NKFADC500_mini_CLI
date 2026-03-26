import os
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QLabel, QLineEdit, QPushButton, QFileDialog

class PathControllerWidget(QWidget):
    """
    [OOP 모듈] 데이터 저장 및 Config 읽기 경로를 독립적으로 제어하는 UI 객체
    DaqTab, ProductionTab 등 어디서든 재사용 가능합니다.
    """
    def __init__(self, label_text, default_path, parent=None):
        super().__init__(parent)
        self.layout = QHBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        
        # 💡 [테마 적용] 박사님의 메인 화이트/베이지 테마 색상과 완벽하게 일치시킴
        self.label = QLabel(label_text)
        self.label.setStyleSheet("font-weight: bold; color: #333333;")
        
        self.path_input = QLineEdit(default_path)
        self.path_input.setStyleSheet("""
            background-color: #FFFFFF; 
            color: #333333; 
            border: 1px solid #CDBA96; 
            border-radius: 4px; 
            padding: 6px;
            font-family: Consolas;
        """)
        # 직접 타이핑하다가 오타로 인해 경로가 꼬이는 것을 방지. 무조건 Browse 버튼을 쓰게 유도
        self.path_input.setReadOnly(True) 
        
        self.btn_browse = QPushButton("📂 Browse")
        self.btn_browse.setStyleSheet("""
            background-color: #E8E2D2; 
            color: #333333; 
            font-weight: bold; 
            border: 1px solid #A69B8D; 
            border-radius: 4px; 
            padding: 6px 15px;
        """)
        self.btn_browse.clicked.connect(self.browse_directory)
        
        self.layout.addWidget(self.label)
        self.layout.addWidget(self.path_input, stretch=1)
        self.layout.addWidget(self.btn_browse)

    def browse_directory(self):
        # 파일 다이얼로그를 열어 사용자가 직접 GUI로 폴더를 선택하도록 함
        dir_path = QFileDialog.getExistingDirectory(self, "Select Directory", self.path_input.text())
        if dir_path:
            self.path_input.setText(dir_path)

    def get_path(self):
        # 다른 탭(DaqTab, ProductionTab)에서 설정된 경로를 문자열로 빼갈 때 사용하는 Getter 메서드
        return self.path_input.text().strip()
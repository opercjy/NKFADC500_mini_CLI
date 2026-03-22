import sys
import os
from PyQt5.QtWidgets import QApplication
from windows.MainWindow import MainWindow

def main():
    # 💡 [버그픽스] 최신 리눅스 환경의 불필요한 Wayland 경고 로그 완전 제거
    os.environ["QT_QPA_PLATFORM"] = "xcb"
    
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
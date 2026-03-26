import sys
import os
from PyQt5.QtWidgets import QApplication
from windows.MainWindow import MainWindow

def main():
    app = QApplication(sys.argv)
    
    # 💡 터미널에서 'fadc500_gui .' 처럼 인자를 주었는지 확인
    target_dir = None
    if len(sys.argv) > 1:
        target_dir = os.path.abspath(sys.argv[1])
        
    # 인자가 있다면 MainWindow로 타겟 경로를 넘겨줌
    window = MainWindow(target_dir=target_dir)
    window.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()
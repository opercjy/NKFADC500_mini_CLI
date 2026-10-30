import os
import sys

# Ensure gui/ is always on sys.path even when invoked from outside gui/
GUI_DIR = os.path.dirname(os.path.abspath(__file__))
if GUI_DIR not in sys.path:
    sys.path.insert(0, GUI_DIR)

from PySide6.QtWidgets import QApplication
from windows.MainWindow import MainWindow


def main():
    app = QApplication(sys.argv)

    target_dir = None
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        target_dir = os.path.abspath(sys.argv[1])

    window = MainWindow(target_dir=target_dir)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[INFO] Installing udev rules for NKFADC500 (priority 99, after 50-udev-default.rules)..."
sudo rm -f /etc/udev/rules.d/50-nkfadc500.rules
sudo cp "${SCRIPT_DIR}/50-nkfadc500.rules" /etc/udev/rules.d/99-nkfadc500.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb
echo "[SUCCESS] USB udev rules (/etc/udev/rules.d/99-nkfadc500.rules) applied successfully!"
echo "Normal users can now access NKFADC500 without sudo."

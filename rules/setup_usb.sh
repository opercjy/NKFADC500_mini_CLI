#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[INFO] Copying udev rules for NKFADC500..."
sudo cp "${SCRIPT_DIR}/50-nkfadc500.rules" /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
echo "[SUCCESS] USB udev rules applied successfully!"
echo "Normal users can now access NKFADC500 without sudo."

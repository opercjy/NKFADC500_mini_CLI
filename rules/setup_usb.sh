#!/bin/bash
echo "[INFO] Copying udev rules for NKFADC500..."
sudo cp 50-nkfadc500.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
echo "[SUCCESS] USB udev rules applied successfully!"
echo "Normal users can now access NKFADC500 without sudo."

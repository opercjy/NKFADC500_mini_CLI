#!/usr/bin/env bash
# =========================================================================
# NKFADC500 Mini CLI & GUI Environment Setup Script
# Usage: source setup.sh
# =========================================================================

export NKFADC500_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="${NKFADC500_ROOT}/bin:${PATH}"
export LD_LIBRARY_PATH="${NKFADC500_ROOT}/lib${NKHOME:+:${NKHOME}/lib}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

echo -e "\033[1;32m[SYSTEM] NKFADC500_ROOT set to: ${NKFADC500_ROOT}\033[0m"
echo -e "\033[1;36m[SYSTEM] Added ${NKFADC500_ROOT}/bin to PATH and ${NKFADC500_ROOT}/lib to LD_LIBRARY_PATH.\033[0m"
echo -e "\033[1;33m[TIP] Launch the GUI from anywhere via:\033[0m"
echo -e "\033[1;37m      fadc500_gui [optional_workspace_dir]\033[0m"

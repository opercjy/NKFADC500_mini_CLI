# ==============================================================================
# NKFADC500 Mini - GUI Launcher CMakeLists.txt
# Creates bin/fadc500_gui pointing directly to the Single-Source-Of-Truth gui/
# ==============================================================================
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(WRITE ${CMAKE_SOURCE_DIR}/bin/fadc500_gui
"#!/usr/bin/env bash
set -e
SOURCE_PATH=\"\$(readlink -f \"\${BASH_SOURCE[0]}\" 2>/dev/null || realpath \"\${BASH_SOURCE[0]}\" 2>/dev/null || echo \"\${BASH_SOURCE[0]}\")\"
SCRIPT_DIR=\"\$(cd \"\$(dirname \"\${SOURCE_PATH}\")\" && pwd)\"
export NKFADC500_ROOT=\"\$(cd \"\${SCRIPT_DIR}/..\" && pwd)\"
NOTICE_LIB=\"\${NKHOME:-/usr/local/notice}/lib\"
export LD_LIBRARY_PATH=\"\${NKFADC500_ROOT}/lib:\${NOTICE_LIB}\${LD_LIBRARY_PATH:+:\${LD_LIBRARY_PATH}}\"
if [ -x \"\${NKFADC500_ROOT}/.venv/bin/python3\" ]; then
    PYTHON_BIN=\"\${NKFADC500_ROOT}/.venv/bin/python3\"
else
    PYTHON_BIN=\"python3\"
fi
exec \"\${PYTHON_BIN}\" \"\${NKFADC500_ROOT}/gui/main.py\" \"\$@\"
")

execute_process(COMMAND chmod +x ${CMAKE_SOURCE_DIR}/bin/fadc500_gui)
message(STATUS "[INFO] GUI launcher generated at ${CMAKE_SOURCE_DIR}/bin/fadc500_gui")

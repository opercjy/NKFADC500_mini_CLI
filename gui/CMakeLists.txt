# ==============================================================================
# NKFADC500 Mini - GUI Launcher CMakeLists.txt
# Creates bin/fadc500_gui pointing directly to the Single-Source-Of-Truth gui/
# ==============================================================================
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(WRITE ${CMAKE_SOURCE_DIR}/bin/fadc500_gui
"#!/usr/bin/env bash
set -e
SCRIPT_DIR=\"\$(cd \"\$(dirname \"\${BASH_SOURCE[0]}\")\" && pwd)\"
export NKFADC500_ROOT=\"\$(cd \"\${SCRIPT_DIR}/..\" && pwd)\"
export LD_LIBRARY_PATH=\"\${NKFADC500_ROOT}/lib\${NKHOME:+:\${NKHOME}/lib}\${LD_LIBRARY_PATH:+:\${LD_LIBRARY_PATH}}\"
exec python3 \"\${NKFADC500_ROOT}/gui/main.py\" \"\$@\"
")

execute_process(COMMAND chmod +x ${CMAKE_SOURCE_DIR}/bin/fadc500_gui)
message(STATUS "[INFO] GUI launcher generated at ${CMAKE_SOURCE_DIR}/bin/fadc500_gui")

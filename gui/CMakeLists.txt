# ==============================================================================
# NKFADC500 Mini - GUI CMakeLists.txt
# Python 스크립트들을 최상위 bin/gui 폴더로 복사하고 구동 쉘 스크립트 생성
# ==============================================================================

# 💡 [버그픽스] CMAKE_BINARY_DIR(build)가 아닌 CMAKE_SOURCE_DIR(최상위)의 bin으로 타겟 변경
set(GUI_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/gui)

# 디렉토리 구조 그대로 복사
file(COPY main.py windows widgets core DESTINATION ${GUI_OUTPUT_DIR})

# 최상위 bin/ 폴더에 GUI를 즉시 실행할 수 있는 단축 쉘 스크립트 생성
file(WRITE ${CMAKE_SOURCE_DIR}/bin/fadc500_gui 
"#!/bin/bash
cd \$(dirname \$0)/gui
python3 main.py \"\$@\"
")

# 스크립트에 실행 권한 부여
execute_process(COMMAND chmod +x ${CMAKE_SOURCE_DIR}/bin/fadc500_gui)

message(STATUS "✅ GUI Python modules mapped to ${GUI_OUTPUT_DIR}")
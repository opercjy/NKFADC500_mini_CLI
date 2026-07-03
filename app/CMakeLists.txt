# ==========================================
# app/CMakeLists.txt
# ==========================================
include_directories(
    ${CMAKE_SOURCE_DIR}/objects/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${ROOT_INCLUDE_DIRS}
    /usr/local/notice/include
)

# [핵심 버그픽스] 외부 하드웨어 제어 라이브러리 참조 경로 우선 선언
link_directories(/usr/local/notice/lib)

# ------------------------------------------------------------------------------
# 1. Frontend DAQ 
# ------------------------------------------------------------------------------
add_executable(frontend_nkfadc500 frontend_main.cpp)
target_link_libraries(frontend_nkfadc500 FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# ------------------------------------------------------------------------------
# 2. Production (Offline Parser)
# ------------------------------------------------------------------------------
add_executable(production_nkfadc_500 production_main.cpp)
target_link_libraries(production_nkfadc_500 FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# ------------------------------------------------------------------------------
# 3. Online Monitor
# ------------------------------------------------------------------------------
add_executable(online_nkfadc500 online_monitor.cpp)
target_link_libraries(online_nkfadc500 FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# ------------------------------------------------------------------------------
# 단일 진실 공급원(SSOT) 타겟 디렉토리 강제 할당
# ------------------------------------------------------------------------------
set_target_properties(
    frontend_nkfadc500 
    production_nkfadc_500 
    online_nkfadc500
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
# ==========================================
# app/CMakeLists.txt
# ==========================================
include_directories(
    ${CMAKE_SOURCE_DIR}/objects/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${ROOT_INCLUDE_DIRS}
    /usr/local/notice/include
)

# 💡 [핵심 버그픽스] 반드시 add_executable 전에 선언해야 링커가 경로를 찾습니다.
link_directories(/usr/local/notice/lib)

# 1. Frontend DAQ
add_executable(frontend_500_mini frontend_main.cpp)
target_link_libraries(frontend_500_mini FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# 2. Production (Offline Parser)
add_executable(production_500_mini production_main.cpp)
target_link_libraries(production_500_mini FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# 3. Online Monitor (이름 변경 반영)
add_executable(online_monitor online_monitor.cpp)
target_link_libraries(online_monitor FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# 실행 파일을 bin 폴더로 일괄 복사
set_target_properties(frontend_500_mini production_500_mini online_monitor
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
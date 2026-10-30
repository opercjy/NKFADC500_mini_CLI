# ==========================================
# app/CMakeLists.txt
# ==========================================

# 1. Frontend DAQ (USB 3.0 Hardware Readout Producer-Consumer Engine)
add_executable(frontend_nkfadc500 frontend_main.cpp)
target_link_libraries(frontend_nkfadc500 PRIVATE FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# 2. Production (High-Throughput Offline Binary-to-ROOT Converter & -d Viewer)
add_executable(production_nkfadc_500 production_main.cpp)
target_link_libraries(production_nkfadc_500 PRIVATE FADC500Core FADC500Objects ${ROOT_LIBRARIES})

# 3. Online Monitor (Live ROOT Canvas Stream Tailer)
add_executable(online_nkfadc500 online_monitor.cpp)
target_link_libraries(online_nkfadc500 PRIVATE FADC500Core FADC500Objects ${ROOT_LIBRARIES})

set_target_properties(
    frontend_nkfadc500
    production_nkfadc_500
    online_nkfadc500
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

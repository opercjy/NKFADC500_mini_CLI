# ==============================================================================
# Applications (Frontend, Production, Viewer) CMake
# ==============================================================================

# 1. Frontend (Online DAQ) : 초고속 바이너리 수집기
# - 하드웨어 제어를 위해 Notice 제조사 라이브러리 및 libusb-1.0 강력 링크
add_executable(frontend_500_mini 
    frontend_main.cpp 
    DaqSystem.cpp
)
target_include_directories(frontend_500_mini PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../objects/include)
target_link_libraries(frontend_500_mini PRIVATE 
    NoticeNKFADC500 
    usb3com 
    nkusb 
    usb-1.0 
    FADC500Objects  # 설정 파일(RunInfo) 파싱 용도
    ${ROOT_LIBRARIES}
)

# 2. Production (Offline) : 바이너리 -> ROOT 변환 및 파형 분석기
add_executable(production_500_mini 
    production_main.cpp 
    Processor.cpp
)
target_include_directories(production_500_mini PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../objects/include)
target_link_libraries(production_500_mini PRIVATE 
    FADC500Objects 
    ${ROOT_LIBRARIES}
)

# 3. Visualize : ROOT 파형 및 히스토그램 뷰어
add_executable(visualize_waveforms 
    visualize_waveforms.cpp
)
target_include_directories(visualize_waveforms PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../objects/include)
target_link_libraries(visualize_waveforms PRIVATE 
    FADC500Objects 
    ${ROOT_LIBRARIES}
)

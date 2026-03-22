# ==============================================================================
# Applications (Frontend, Production, Viewer) CMake
# ==============================================================================

# 1. Frontend (Online DAQ) : 초고속 바이너리 수집기 (OOP 기반)
add_executable(frontend_500_mini 
    frontend_main.cpp 
    BinaryDaqManager.cpp
    Fadc500Device.cpp
    # (ELog.cc, ConfigParser.cc 등은 추후 필요시 여기에 추가)
)
target_include_directories(frontend_500_mini PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../objects/include)
target_link_libraries(frontend_500_mini PRIVATE 
    NoticeNKFADC500 
    usb3com 
    nkusb 
    usb-1.0 
    FADC500Objects
    ${ROOT_LIBRARIES}
)

# 2. Production (Offline) : 바이너리 -> ROOT 변환기 (뼈대)
add_executable(production_500_mini 
    production_main.cpp 
    # Processor.cpp
)
target_include_directories(production_500_mini PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../objects/include)
target_link_libraries(production_500_mini PRIVATE FADC500Objects ${ROOT_LIBRARIES})

# 3. Visualize : 뷰어 (뼈대)
add_executable(visualize_waveforms 
    visualize_waveforms.cpp
)
target_include_directories(visualize_waveforms PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../objects/include)
target_link_libraries(visualize_waveforms PRIVATE FADC500Objects ${ROOT_LIBRARIES})

cmake_minimum_required(VERSION 3.10)
project(Fadc500Project CXX)

# C++14 표준 사용
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 실행 파일 및 라이브러리 출력 경로 설정 ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- 설치 경로 설정 ---
if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Default install path" FORCE)
endif()
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# --- 필수 패키지 및 라이브러리 경로 설정 ---
find_package(ROOT REQUIRED)
include(${ROOT_USE_FILE})
include_directories(include)
include_directories($ENV{NKHOME}/include)
link_directories($ENV{NKHOME}/lib)

# ==============================================================================
# 빌드 타겟 1: 데이터 획득 프로그램 (frontend_500_mini)
# ==============================================================================
add_executable(frontend_500_mini src/frontend_main.cpp src/DaqSystem.cpp)
target_link_libraries(frontend_500_mini
    PRIVATE
    ${ROOT_LIBRARIES}
    NoticeNKFADC500ROOT
    usb3comroot
)
install(TARGETS frontend_500_mini DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/frontend_500_mini)


# ==============================================================================
# 빌드 타겟 2: 데이터 분석/후처리 프로그램 (production_500_mini)
# ==============================================================================
add_executable(production_500_mini src/production_main.cpp src/Processor.cpp)
target_link_libraries(production_500_mini
    PRIVATE
    ${ROOT_LIBRARIES}
)
install(TARGETS production_500_mini DESTINATION bin)


# ==============================================================================
# 빌드 타겟 3: 데이터 시각화 프로그램 (visualize_waveforms)
# ==============================================================================
add_executable(visualize_waveforms src/visualize_waveforms.cpp)
target_link_libraries(visualize_waveforms
    PRIVATE
    ${ROOT_LIBRARIES}
)
install(TARGETS visualize_waveforms DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION etc/scripts)


# ==============================================================================
# 제거 타겟 정의
# ==============================================================================
add_custom_target(uninstall
    COMMAND xargs rm -vrf < ${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt
    COMMENT "Uninstalling the project from ${CMAKE_INSTALL_PREFIX}..."
)

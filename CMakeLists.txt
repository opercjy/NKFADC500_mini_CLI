# ==============================================================================
# NKFADC500_mini_CLI : NoticeDAQ Standalone Control Build Script
# ==============================================================================
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(NKFADC500_mini CXX)

# [출력 경로 설정] 실행 파일은 bin/, 라이브러리와 딕셔너리는 lib/ 로 자동 배치
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# [컴파일러 옵션] C++17 표준 및 최적화
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -O3)

# 1. CERN ROOT 라이브러리 로드
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree Net Thread Gui MathCore Hist)
include(${ROOT_USE_FILE})

# 2. 제조사 라이브러리(NKHOME) 환경 변수 검증 및 경로 연동
if(NOT DEFINED ENV{NKHOME})
    message(FATAL_ERROR "[ERROR] NKHOME is not set! Please run: source notice_nkfadc500_4/notice/notice_env.sh")
endif()
set(NKHOME $ENV{NKHOME})

# 전역 헤더 및 라이브러리 탐색 경로
include_directories(${ROOT_INCLUDE_DIRS} ${NKHOME}/include)
link_directories(${NKHOME}/lib)

# 3. 하위 모듈 빌드 트리
add_subdirectory(objects)
add_subdirectory(src)

message(STATUS "✅ NKFADC500_mini CMake configuration completed successfully.")

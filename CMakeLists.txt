# ==============================================================================
# NKFADC500_mini_CLI : NoticeDAQ Standalone Control & Offline Production Suite
# ==============================================================================
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(NKFADC500_mini LANGUAGES C CXX)

# Standard install directories (required by ROOT 6.32+ dictionary macros)
include(GNUInstallDirs)
set(CMAKE_INSTALL_LIBDIR "lib")

# Output binaries to bin/ and shared libraries/PCM dictionaries to lib/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# C/C++ Standards & Compiler Warnings
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -O3)

# Embed relative RPATH so executables in bin/ automatically locate ../lib/*.so
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_RPATH "$ORIGIN/../lib")

# 1. CERN ROOT
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree Net Thread Gui MathCore Hist)
include(${ROOT_USE_FILE})

# 2. libusb-1.0 Discovery (supports system /usr/include/libusb-1.0, pkg-config, and Conda)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_LIBUSB QUIET libusb-1.0)
endif()

find_path(LIBUSB_INCLUDE_DIR
    NAMES libusb.h
    HINTS ${PC_LIBUSB_INCLUDE_DIRS} $ENV{LIBUSB_INC} $ENV{CONDA_PREFIX}/include
    PATH_SUFFIXES libusb-1.0
)
find_library(LIBUSB_LIBRARY
    NAMES usb-1.0
    HINTS ${PC_LIBUSB_LIBRARY_DIRS} $ENV{LIBUSB_LIB} $ENV{CONDA_PREFIX}/lib
)

if(NOT LIBUSB_INCLUDE_DIR OR NOT LIBUSB_LIBRARY)
    message(FATAL_ERROR "[ERROR] libusb-1.0 headers/library not found. Install libusb-1.0-0-dev (Ubuntu) or libusb1-devel (RHEL/Rocky).")
endif()
message(STATUS "[INFO] Found libusb-1.0: ${LIBUSB_LIBRARY} (include: ${LIBUSB_INCLUDE_DIR})")

# 3. Notice Korea NKFADC500 SDK Resolution:
#    Use pre-installed $NKHOME (or /usr/local/notice) if libraries exist;
#    otherwise automatically build bundled notice_nkfadc500_4 C libraries into lib/!
set(CANDIDATE_NKHOME "")
if(DEFINED ENV{NKHOME} AND EXISTS "$ENV{NKHOME}/lib/libNoticeNKFADC500.so")
    set(CANDIDATE_NKHOME "$ENV{NKHOME}")
elseif(EXISTS "/usr/local/notice/lib/libNoticeNKFADC500.so")
    set(CANDIDATE_NKHOME "/usr/local/notice")
endif()

if(CANDIDATE_NKHOME)
    message(STATUS "[INFO] Using pre-installed Notice SDK at: ${CANDIDATE_NKHOME}")
    set(NOTICE_INCLUDE_DIRS "${CANDIDATE_NKHOME}/include")
    set(NOTICE_LIBRARIES
        "${CANDIDATE_NKHOME}/lib/libNoticeNKFADC500.so"
        "${CANDIDATE_NKHOME}/lib/libusb3com.so"
        "${CANDIDATE_NKHOME}/lib/libnkusb.so"
        "${LIBUSB_LIBRARY}"
    )
    list(APPEND CMAKE_BUILD_RPATH "${CANDIDATE_NKHOME}/lib")
else()
    set(VENDOR_SRC_ROOT "${CMAKE_SOURCE_DIR}/notice_nkfadc500_4/notice/src")
    if(NOT EXISTS "${VENDOR_SRC_ROOT}/nkfadc500/nkfadc500/NoticeNKFADC500.c")
        message(FATAL_ERROR "[ERROR] Notice SDK not found in NKHOME and bundled source is missing under ${VENDOR_SRC_ROOT}.")
    endif()
    message(STATUS "[INFO] Pre-installed NKHOME not found; building bundled Notice NKFADC500 C SDK into ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

    add_library(nkusb SHARED "${VENDOR_SRC_ROOT}/nkusb/nkusb/nkusb.c")
    target_include_directories(nkusb PUBLIC "${VENDOR_SRC_ROOT}/nkusb/nkusb" "${LIBUSB_INCLUDE_DIR}")
    target_compile_options(nkusb PRIVATE -Wno-unused-variable -Wno-sign-compare)
    target_link_libraries(nkusb PUBLIC "${LIBUSB_LIBRARY}")

    add_library(usb3com SHARED "${VENDOR_SRC_ROOT}/usb3com/usb3com/usb3com.c")
    target_include_directories(usb3com PUBLIC "${VENDOR_SRC_ROOT}/usb3com/usb3com" "${LIBUSB_INCLUDE_DIR}")
    target_compile_options(usb3com PRIVATE -Wno-unused-variable -Wno-sign-compare)
    target_link_libraries(usb3com PUBLIC nkusb "${LIBUSB_LIBRARY}")

    add_library(NoticeNKFADC500 SHARED "${VENDOR_SRC_ROOT}/nkfadc500/nkfadc500/NoticeNKFADC500.c")
    target_include_directories(NoticeNKFADC500 PUBLIC "${VENDOR_SRC_ROOT}/nkfadc500/nkfadc500" "${VENDOR_SRC_ROOT}/usb3com/usb3com" "${LIBUSB_INCLUDE_DIR}")
    target_compile_options(NoticeNKFADC500 PRIVATE -Wno-unused-variable -Wno-sign-compare -Wno-pointer-sign)
    target_link_libraries(NoticeNKFADC500 PUBLIC usb3com nkusb "${LIBUSB_LIBRARY}")

    set(NOTICE_INCLUDE_DIRS
        "${VENDOR_SRC_ROOT}/nkfadc500/nkfadc500"
        "${VENDOR_SRC_ROOT}/usb3com/usb3com"
        "${VENDOR_SRC_ROOT}/nkusb/nkusb"
    )
    set(NOTICE_LIBRARIES NoticeNKFADC500 usb3com nkusb "${LIBUSB_LIBRARY}")
endif()

# 4. Optional ZeroMQ + cppzmq Discovery (for legacy ZMQ online_nkfadc500 publisher)
find_path(ZMQ_HPP_INCLUDE_DIR NAMES zmq.hpp HINTS $ENV{CONDA_PREFIX}/include)
find_library(ZMQ_LIBRARY NAMES zmq HINTS $ENV{CONDA_PREFIX}/lib)

# 5. Subdirectories
add_subdirectory(objects)
add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(gui)

message(STATUS "NKFADC500_mini CMake configuration completed successfully.")

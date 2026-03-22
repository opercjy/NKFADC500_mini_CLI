# ==========================================
# core/CMakeLists.txt
# ==========================================
include_directories(
    ${CMAKE_SOURCE_DIR}/objects/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ROOT_INCLUDE_DIRS}
    /usr/local/notice/include
)

set(CORE_SOURCES
    src/BinaryDaqManager.cpp
    src/Fadc500Device.cpp
    src/ConfigParser.cpp
    src/ELog.cpp
)

# Core 기능들을 정적 라이브러리(libFADC500Core.a)로 묶음
add_library(FADC500Core STATIC ${CORE_SOURCES})

# 💡 링커가 길을 잃지 않도록 제조사 .so 파일 절대 경로를 직접 매핑
set(NOTICE_LIB "/usr/local/notice/lib")

target_link_libraries(FADC500Core
    PUBLIC
    FADC500Objects
    ${ROOT_LIBRARIES}
    ${NOTICE_LIB}/libNoticeNKFADC500.so
    ${NOTICE_LIB}/libNoticeNKFADC500ROOT.so
    ${NOTICE_LIB}/libnkusb.so
    ${NOTICE_LIB}/libNKUSBROOT.so        # <--- 대문자로 수정됨!
    ${NOTICE_LIB}/libusb3com.so
    ${NOTICE_LIB}/libusb3comroot.so
)
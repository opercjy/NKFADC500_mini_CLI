# ==========================================
# core/CMakeLists.txt
# ==========================================
set(CORE_SOURCES
    src/BinaryDaqManager.cpp
    src/Fadc500Device.cpp
    src/ConfigParser.cpp
    src/ELog.cpp
)

add_library(FADC500Core STATIC ${CORE_SOURCES})

target_include_directories(FADC500Core
    PUBLIC
    ${CMAKE_SOURCE_DIR}/objects/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ROOT_INCLUDE_DIRS}
    ${NOTICE_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIR}
)

find_package(Threads REQUIRED)

target_link_libraries(FADC500Core
    PUBLIC
    FADC500Objects
    ${ROOT_LIBRARIES}
    ${NOTICE_LIBRARIES}
    Threads::Threads
)

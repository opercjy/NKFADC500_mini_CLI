# ==============================================================================
# Objects / ROOT Dictionary CMake
# ==============================================================================

file(GLOB SOURCES "src/*.cc")
file(GLOB HEADERS "include/*.hh")

# 1. 공유 라이브러리 생성 및 최신 ROOT 6 딕셔너리 연결 (MODULE 지정으로 rootmap/pcm과 .so 이름 일치)
add_library(FADC500Objects SHARED ${SOURCES})
target_include_directories(FADC500Objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(FADC500Objects PUBLIC ROOT::Core ROOT::RIO ROOT::Hist ROOT::Tree ROOT::MathCore)

root_generate_dictionary(FADC500ObjectsDict
    ${HEADERS}
    MODULE FADC500Objects
    LINKDEF include/LinkDef.h
)
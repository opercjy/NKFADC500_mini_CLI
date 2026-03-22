# ==============================================================================
# Objects / ROOT Dictionary CMake
# ==============================================================================

file(GLOB SOURCES "src/*.cc")
file(GLOB HEADERS "include/*.hh")

# 1. 최신 ROOT 6 딕셔너리 생성
# (ROOT 6.32는 이 단계에서 자동으로 pcm 파일을 CMAKE_LIBRARY_OUTPUT_DIRECTORY(lib/)로 보냅니다)
root_generate_dictionary(FADC500ObjectsDict 
    ${HEADERS} 
    LINKDEF include/LinkDef.h
)

# 2. 공유 라이브러리 생성
add_library(FADC500Objects SHARED ${SOURCES} FADC500ObjectsDict.cxx)

# 헤더 참조 경로 노출 및 ROOT 코어 라이브러리 링크
target_include_directories(FADC500Objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(FADC500Objects PUBLIC ROOT::Core ROOT::RIO ROOT::Hist ROOT::Tree ROOT::MathCore)
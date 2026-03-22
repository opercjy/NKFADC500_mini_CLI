# ==============================================================================
# Objects / ROOT Dictionary CMake
# ==============================================================================

file(GLOB SOURCES "src/*.cc")
file(GLOB HEADERS "include/*.hh")

# 1. 최신 ROOT 6 딕셔너리 생성
root_generate_dictionary(FADC500ObjectsDict 
    ${HEADERS} 
    LINKDEF include/LinkDef.h
)

# 2. 공유 라이브러리 생성
add_library(FADC500Objects SHARED ${SOURCES} FADC500ObjectsDict.cxx)

# 헤더 참조 경로 노출 및 ROOT 코어 라이브러리 링크
target_include_directories(FADC500Objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(FADC500Objects PUBLIC ROOT::Core ROOT::RIO ROOT::Hist ROOT::Tree ROOT::MathCore)

# 3. 런타임 에러 방지: 생성된 PCM 파일을 lib/ 폴더로 복사
add_custom_command(TARGET FADC500Objects POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libFADC500ObjectsDict_rdict.pcm ${CMAKE_SOURCE_DIR}/lib/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libFADC500ObjectsDict.rootmap ${CMAKE_SOURCE_DIR}/lib/
    COMMENT "Deploying FADC500Objects dictionaries to lib/ directory."
)

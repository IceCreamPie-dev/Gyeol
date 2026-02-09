# 생성된 헤더 파일의 경로를 변수로 가져옵니다
set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/src/gyeol_core/include/generated")

# Parser 라이브러리 (테스트에서도 사용)
add_library(GyeolParser
    gyeol_parser.h
    gyeol_parser.cpp
    gyeol_comp_analyzer.h
    gyeol_comp_analyzer.cpp
    gyeol_json_export.h
    gyeol_json_export.cpp
)

target_include_directories(GyeolParser PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src/gyeol_core/include"
    "${GENERATED_DIR}"
)

target_link_libraries(GyeolParser PUBLIC flatbuffers nlohmann_json::nlohmann_json)
add_dependencies(GyeolParser GyeolCore)

# Compiler 실행 파일
add_executable(GyeolCompiler compiler_main.cpp)
target_link_libraries(GyeolCompiler PRIVATE GyeolParser)

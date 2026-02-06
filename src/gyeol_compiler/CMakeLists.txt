# 생성된 헤더 파일의 경로를 변수로 가져옵니다
set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/src/gyeol_core/include/generated")

add_executable(GyeolCompiler compiler_main.cpp)

# FlatBuffers 및 생성된 헤더 경로 포함
target_include_directories(GyeolCompiler PUBLIC
    "${CMAKE_SOURCE_DIR}/src/gyeol_core/include"
    "${GENERATED_DIR}"
)

# FlatBuffers 라이브러리 링크
target_link_libraries(GyeolCompiler PRIVATE flatbuffers)

# 컴파일러 실행 시 gyeol_generated.h가 반드시 먼저 생성되어 있어야 함
add_dependencies(GyeolCompiler GyeolCore)

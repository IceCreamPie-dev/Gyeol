# --- 1. 변수 설정 ---
set(SCHEMA_DIR "${CMAKE_SOURCE_DIR}/schemas")
set(SCHEMA_FILE "${SCHEMA_DIR}/gyeol.fbs")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/generated")

# --- 2. FlatBuffers 컴파일 명령 ---
file(MAKE_DIRECTORY ${GENERATED_DIR})

# FetchContent를 썼으므로 'flatc' 타겟을 바로 사용할 수 있습니다.
# (Windows에서는 .exe 확장자가 자동으로 처리됨)
add_custom_command(
    OUTPUT "${GENERATED_DIR}/gyeol_generated.h"
    COMMAND flatc # flatbuffers::flatc 타겟이 만든 실행 파일
    ARGS --cpp --cpp-std c++17 --gen-object-api -o "${GENERATED_DIR}" "${SCHEMA_FILE}"
    DEPENDS "${SCHEMA_FILE}" flatc # flatc가 먼저 빌드되어야 함
    COMMENT "Compiling FlatBuffers schema: ${SCHEMA_FILE}"
)

# --- 3. 라이브러리 타겟 생성 ---
add_library(GyeolCore
    src/gyeol_story.cpp
    src/gyeol_runner.cpp
    include/gyeol_story.h
    include/gyeol_runner.h
    "${GENERATED_DIR}/gyeol_generated.h"
)

# --- 4. Include 및 Link 설정 ---
target_include_directories(GyeolCore PUBLIC
    include
    "${GENERATED_DIR}"
)

# FetchContent로 가져온 flatbuffers 라이브러리와 링크
target_link_libraries(GyeolCore PUBLIC flatbuffers)

# --- 5. 테스트용 실행 파일 ---
add_executable(GyeolTest src/test_main.cpp)
target_link_libraries(GyeolTest PRIVATE GyeolCore)
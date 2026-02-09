cmake_minimum_required(VERSION 3.15)
project(GyeolWasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- FlatBuffers (소스 빌드, flatc 불필요) ---
include(FetchContent)

FetchContent_Declare(
    flatbuffers
    GIT_REPOSITORY https://github.com/google/flatbuffers.git
    GIT_TAG        v24.3.25
)

set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "" FORCE)  # WASM에서는 flatc 불필요
set(FLATBUFFERS_BUILD_FLATHASH OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(flatbuffers)

# --- nlohmann/json (JSON IR export) ---
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# --- 소스 경로 ---
set(CORE_DIR "${CMAKE_SOURCE_DIR}/../../src/gyeol_core")
set(COMPILER_DIR "${CMAKE_SOURCE_DIR}/../../src/gyeol_compiler")
set(GENERATED_DIR "${CORE_DIR}/include/generated")

# --- GyeolCore 라이브러리 (Runner + Story) ---
add_library(GyeolCore STATIC
    ${CORE_DIR}/src/gyeol_story.cpp
    ${CORE_DIR}/src/gyeol_runner.cpp
)

target_include_directories(GyeolCore PUBLIC
    ${CORE_DIR}/include
    ${GENERATED_DIR}
)

target_link_libraries(GyeolCore PUBLIC flatbuffers)

# --- GyeolParser 라이브러리 (Parser + Analyzer + JSON Export) ---
add_library(GyeolParser STATIC
    ${COMPILER_DIR}/gyeol_parser.cpp
    ${COMPILER_DIR}/gyeol_comp_analyzer.cpp
    ${COMPILER_DIR}/gyeol_json_export.cpp
)

target_include_directories(GyeolParser PUBLIC
    ${COMPILER_DIR}
    ${CORE_DIR}/include
    ${GENERATED_DIR}
)

target_link_libraries(GyeolParser PUBLIC flatbuffers nlohmann_json::nlohmann_json)

# --- WASM 바인딩 실행 파일 ---
add_executable(gyeol gyeol_wasm.cpp)

target_link_libraries(gyeol PRIVATE GyeolCore GyeolParser)

target_include_directories(gyeol PRIVATE
    ${CORE_DIR}/include
    ${COMPILER_DIR}
    ${GENERATED_DIR}
)

# Emscripten 설정
set_target_properties(gyeol PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "\
        --bind \
        -s MODULARIZE=1 \
        -s EXPORT_NAME='createGyeolModule' \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s ENVIRONMENT=web,worker \
        -s NO_EXIT_RUNTIME=1 \
        -s SINGLE_FILE=0 \
        -s WASM=1 \
        -O2 \
    "
)

# 출력 디렉터리 설정
set_target_properties(gyeol PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)

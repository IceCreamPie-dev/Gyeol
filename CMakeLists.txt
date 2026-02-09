cmake_minimum_required(VERSION 3.15)
project(Gyeol VERSION 0.1.0 LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- [핵심 변경] FlatBuffers 자동 다운로드 및 설정 (FetchContent) ---
include(FetchContent)

message(STATUS "Fetching FlatBuffers... (This may take a while on first build)")

FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG        v24.3.25  # 최신 안정화 버전 태그 지정
)

# FlatBuffers 빌드 옵션 (테스트/설치 제외하여 속도 향상)
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE) # 컴파일러(flatc)도 같이 빌드

FetchContent_MakeAvailable(flatbuffers)
# ------------------------------------------------------------------

# --- nlohmann/json (FetchContent) ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# --- Google Test (FetchContent) ---
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# 하위 디렉토리 추가
add_subdirectory(src/gyeol_core)
add_subdirectory(src/gyeol_compiler)
add_subdirectory(src/tests)
add_subdirectory(src/gyeol_lsp)
add_subdirectory(src/gyeol_debugger)
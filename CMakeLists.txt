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

# 하위 디렉토리 추가
add_subdirectory(src/gyeol_core)
Add-Content -Path "CMakeLists.txt" -Value "add_subdirectory(src/gyeol_compiler)"
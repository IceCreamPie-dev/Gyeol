# --- 기존 테스트 (Core + Parser) ---
add_executable(GyeolTests
    test_parser.cpp
    test_runner.cpp
    test_story.cpp
    test_saveload.cpp
    test_helpers.h
)

target_include_directories(GyeolTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include/generated
    ${CMAKE_SOURCE_DIR}/src/gyeol_compiler
)

target_link_libraries(GyeolTests PRIVATE
    GyeolCore
    GyeolParser
    gtest
    gtest_main
)

# MinGW: 정적 링크로 DLL 의존성 제거
if(MINGW)
    target_link_options(GyeolTests PRIVATE -static)
endif()

add_test(NAME GyeolTests COMMAND GyeolTests)

# --- LSP 테스트 (Analyzer + Server) ---
add_executable(GyeolLSPTests
    test_lsp.cpp
)

target_include_directories(GyeolLSPTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include/generated
    ${CMAKE_SOURCE_DIR}/src/gyeol_compiler
    ${CMAKE_SOURCE_DIR}/src/gyeol_lsp
)

target_link_libraries(GyeolLSPTests PRIVATE
    GyeolCore
    GyeolParser
    nlohmann_json::nlohmann_json
    gtest
    gtest_main
)

# LSP 소스 파일 직접 포함 (라이브러리가 아니므로)
target_sources(GyeolLSPTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gyeol_lsp/lsp_server.cpp
    ${CMAKE_SOURCE_DIR}/src/gyeol_lsp/gyeol_analyzer.cpp
)

if(MINGW)
    target_link_options(GyeolLSPTests PRIVATE -static)
endif()

add_test(NAME GyeolLSPTests COMMAND GyeolLSPTests)

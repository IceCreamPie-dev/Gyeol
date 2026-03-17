# Core + parser + runtime tests
add_executable(GyeolTests
    test_parser.cpp
    test_runner.cpp
    test_story.cpp
    test_saveload.cpp
    test_json_export.cpp
    test_graph_tools.cpp
    test_runtime_contract.cpp
    runtime_contract_harness.cpp
    ${CMAKE_SOURCE_DIR}/bindings/godot_extension/src/gyeol_story_player_adapter.cpp
    test_helpers.h
    runtime_contract_harness.h
)

target_include_directories(GyeolTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include/generated
    ${CMAKE_SOURCE_DIR}/src/gyeol_compiler
    ${CMAKE_SOURCE_DIR}/bindings/godot_extension/src
)

target_compile_definitions(GyeolTests PRIVATE
    GYEOL_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(GyeolTests PRIVATE
    GyeolCore
    GyeolParser
    gtest
    gtest_main
)

if(MINGW)
    target_link_options(GyeolTests PRIVATE -static)
endif()

add_test(NAME GyeolTests COMMAND GyeolTests)

# Runtime contract conformance CLI
add_executable(GyeolRuntimeContractCLI
    runtime_contract_cli.cpp
    runtime_contract_harness.cpp
    ${CMAKE_SOURCE_DIR}/bindings/godot_extension/src/gyeol_story_player_adapter.cpp
    runtime_contract_harness.h
)

target_include_directories(GyeolRuntimeContractCLI PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include
    ${CMAKE_SOURCE_DIR}/src/gyeol_core/include/generated
    ${CMAKE_SOURCE_DIR}/src/gyeol_compiler
    ${CMAKE_SOURCE_DIR}/bindings/godot_extension/src
)

target_compile_definitions(GyeolRuntimeContractCLI PRIVATE
    GYEOL_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(GyeolRuntimeContractCLI PRIVATE
    GyeolCore
    GyeolParser
)

if(MINGW)
    target_link_options(GyeolRuntimeContractCLI PRIVATE -static)
endif()

# LSP tests (analyzer + server)
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

target_sources(GyeolLSPTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gyeol_lsp/lsp_server.cpp
    ${CMAKE_SOURCE_DIR}/src/gyeol_lsp/gyeol_analyzer.cpp
)

if(MINGW)
    target_link_options(GyeolLSPTests PRIVATE -static)
endif()

add_test(NAME GyeolLSPTests COMMAND GyeolLSPTests)

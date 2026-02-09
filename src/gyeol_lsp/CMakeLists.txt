# nlohmann/json은 루트 CMakeLists.txt에서 이미 FetchContent로 가져옴

# --- GyeolLSP 실행 파일 ---
add_executable(GyeolLSP
    lsp_main.cpp
    lsp_server.h
    lsp_server.cpp
    gyeol_analyzer.h
    gyeol_analyzer.cpp
)

target_include_directories(GyeolLSP PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(GyeolLSP PRIVATE
    GyeolParser
    nlohmann_json::nlohmann_json
)

# C++17 필수 (std::filesystem 사용)
target_compile_features(GyeolLSP PRIVATE cxx_std_17)

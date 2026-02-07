# --- GyeolDebugger CLI ---
add_executable(GyeolDebugger
    debugger_main.cpp
    gyeol_debugger.h
    gyeol_debugger.cpp
)

target_include_directories(GyeolDebugger PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src/gyeol_core/include"
    "${CMAKE_SOURCE_DIR}/src/gyeol_core/include/generated"
)

target_link_libraries(GyeolDebugger PRIVATE GyeolCore)

# MinGW: 정적 링크
if(MINGW)
    target_link_options(GyeolDebugger PRIVATE -static)
endif()

add_executable(GyeolTests
    test_parser.cpp
    test_runner.cpp
    test_story.cpp
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

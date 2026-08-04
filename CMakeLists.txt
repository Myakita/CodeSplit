cmake_minimum_required(VERSION 3.25)

project(
    CodeSplit
    VERSION 0.1.0
    DESCRIPTION "A command-line tool for decomposing large C++ source files"
    LANGUAGES CXX
)

option(CODESPLIT_BUILD_TESTS "Build CodeSplit tests" ON)

add_library(codesplit_core
    src/analysis/source_file.cpp
    src/cli/command_line.cpp
    src/reporting/json_report.cpp
    src/reporting/text_report.cpp
)
target_include_directories(codesplit_core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(codesplit_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(codesplit_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(codesplit_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(codesplit app/main.cpp)
target_link_libraries(codesplit PRIVATE codesplit_core)

if(CODESPLIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

cmake_minimum_required(VERSION 3.25)

project(CodeSplitMvpExample LANGUAGES CXX)

add_library(example_core src/calculator.cpp)
target_include_directories(example_core PUBLIC include)
target_compile_features(example_core PUBLIC cxx_std_17)

add_executable(example_app src/main.cpp)
target_link_libraries(example_app PRIVATE example_core)

enable_testing()
add_test(NAME example_app_runs COMMAND example_app)

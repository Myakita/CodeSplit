cmake_minimum_required(VERSION 3.25)

project(
    CodeSplit
    VERSION 0.1.0
    DESCRIPTION "A command-line tool for decomposing large C++ source files"
    LANGUAGES CXX
)

option(CODESPLIT_BUILD_TESTS "Build CodeSplit tests" ON)
option(CODESPLIT_ENABLE_CLANG "Enable Clang LibTooling integration when available" ON)

add_library(codesplit_core
    src/analysis/source_file.cpp
    src/cli/command_line.cpp
    src/planning/move_plan.cpp
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

if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(codesplit_msvc_runtime_library "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    set(codesplit_msvc_runtime_library_defined ON)
else()
    set(codesplit_msvc_runtime_library_defined OFF)
endif()

if(CODESPLIT_ENABLE_CLANG)
    find_package(LLVM CONFIG QUIET)
    find_package(Clang CONFIG QUIET)
endif()

if(CODESPLIT_ENABLE_CLANG AND LLVM_FOUND AND Clang_FOUND)
    if(MSVC)
        set_property(TARGET codesplit_core PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
    endif()

    if(MSVC AND TARGET LLVMDebugInfoPDB)
        get_target_property(llvm_pdb_link_libraries LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
        set(dia_sdk_architecture "$ENV{VSCMD_ARG_TGT_ARCH}")
        if(dia_sdk_architecture STREQUAL "x64")
            set(dia_sdk_architecture "amd64")
        endif()

        find_library(codesplit_diaguids_library
            NAMES diaguids
            HINTS
                "$ENV{VSINSTALLDIR}/DIA SDK/lib/${dia_sdk_architecture}"
                "$ENV{VSINSTALLDIR}/DIA SDK/lib"
        )

        foreach(link_library IN LISTS llvm_pdb_link_libraries)
            if(link_library MATCHES "diaguids\\.lib$" AND NOT EXISTS "${link_library}" AND
               codesplit_diaguids_library)
                list(REMOVE_ITEM llvm_pdb_link_libraries "${link_library}")
                list(PREPEND llvm_pdb_link_libraries "${codesplit_diaguids_library}")
            endif()
        endforeach()

        set_target_properties(LLVMDebugInfoPDB PROPERTIES
            INTERFACE_LINK_LIBRARIES "${llvm_pdb_link_libraries}"
        )
    endif()

    add_library(codesplit_clang
        src/clang/callable_inventory.cpp
        src/clang/compilation_database.cpp
    )
    target_link_libraries(codesplit_clang PRIVATE clangTooling clangIndex)
    target_include_directories(codesplit_clang SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    set(CODESPLIT_HAS_CLANG_TOOLING ON)
    message(STATUS "CodeSplit Clang LibTooling integration enabled")
else()
    if(codesplit_msvc_runtime_library_defined)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "${codesplit_msvc_runtime_library}")
    else()
        unset(CMAKE_MSVC_RUNTIME_LIBRARY)
    endif()

    add_library(codesplit_clang
        src/clang/unavailable_callable_inventory.cpp
        src/clang/unavailable_compilation_database.cpp
    )
    set(CODESPLIT_HAS_CLANG_TOOLING OFF)
    message(STATUS "CodeSplit Clang LibTooling integration unavailable")
endif()

target_include_directories(codesplit_clang
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_compile_features(codesplit_clang PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(codesplit_clang PRIVATE /W4 /permissive-)
else()
    target_compile_options(codesplit_clang PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(codesplit app/main.cpp)
target_link_libraries(codesplit PRIVATE codesplit_core codesplit_clang)
target_compile_definitions(codesplit PRIVATE CODESPLIT_VERSION="${PROJECT_VERSION}")

if(CODESPLIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

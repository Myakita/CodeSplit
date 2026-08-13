function(run_checked description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${description} failed (${command_result})\n${command_output}\n${command_error}"
        )
    endif()
    set(LAST_COMMAND_OUTPUT "${command_output}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${E2E_ROOT}")
file(MAKE_DIRECTORY "${E2E_ROOT}")
file(COPY "${EXAMPLE_SOURCE}/" DESTINATION "${E2E_ROOT}/project")

set(project_root "${E2E_ROOT}/project")
set(build_root "${E2E_ROOT}/build")
set(source_file "${project_root}/src/calculator.cpp")
set(target_file "${project_root}/src/total.cpp")

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${project_root}"
    -B "${build_root}"
    -G "${TEST_GENERATOR}"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
)
if(TEST_MAKE_PROGRAM)
    list(APPEND configure_command "-DCMAKE_MAKE_PROGRAM=${TEST_MAKE_PROGRAM}")
endif()
if(TEST_RC_COMPILER)
    list(APPEND configure_command "-DCMAKE_RC_COMPILER=${TEST_RC_COMPILER}")
endif()
if(TEST_MT)
    list(APPEND configure_command "-DCMAKE_MT=${TEST_MT}")
endif()

run_checked("example configure" ${configure_command})
run_checked("initial example build" "${CMAKE_COMMAND}" --build "${build_root}")
run_checked(
    "move dry run"
    "${CODESPLIT_EXECUTABLE}" dry-run-move "${source_file}"
    --symbol-id "c:@F@calculate_total#I#I#"
    --target "${target_file}"
    --build-path "${build_root}"
    --format json
)
if(NOT LAST_COMMAND_OUTPUT MATCHES "\"status\": \"ready\"")
    message(FATAL_ERROR "dry run did not produce a ready plan\n${LAST_COMMAND_OUTPUT}")
endif()

run_checked(
    "move apply"
    "${CODESPLIT_EXECUTABLE}" apply-move "${source_file}"
    --symbol-id "c:@F@calculate_total#I#I#"
    --target "${target_file}"
    --build-path "${build_root}"
    --confirm
    --format json
)
if(NOT LAST_COMMAND_OUTPUT MATCHES "\"validated\": true")
    message(FATAL_ERROR "move was not validated\n${LAST_COMMAND_OUTPUT}")
endif()
if(NOT LAST_COMMAND_OUTPUT MATCHES "\"build_target\": \"example_core\"")
    message(FATAL_ERROR "CMake target was not detected\n${LAST_COMMAND_OUTPUT}")
endif()

if(NOT EXISTS "${target_file}")
    message(FATAL_ERROR "target source was not created")
endif()
file(READ "${source_file}" source_contents)
if(source_contents MATCHES "int calculate_total")
    message(FATAL_ERROR "definition remains in the source file")
endif()
file(READ "${project_root}/CMakeLists.txt" cmake_contents)
if(NOT cmake_contents MATCHES "target_sources\\(example_core PRIVATE \"src/total.cpp\"\\)")
    message(FATAL_ERROR "target source was not added to CMakeLists.txt")
endif()

run_checked("final example build" "${CMAKE_COMMAND}" --build "${build_root}")
run_checked("example tests" "${CMAKE_CTEST_COMMAND}" --test-dir "${build_root}" --output-on-failure)

file(GLOB transaction_artifacts
    "${project_root}/src/*.codesplit.*"
    "${project_root}/*.codesplit.*"
)
if(transaction_artifacts)
    message(FATAL_ERROR "transaction artifacts remain: ${transaction_artifacts}")
endif()

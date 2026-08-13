add_executable(codesplit_unit_tests unit/command_line_tests.cpp)
target_link_libraries(codesplit_unit_tests PRIVATE codesplit_core)
add_test(NAME codesplit_unit_tests COMMAND codesplit_unit_tests)

add_executable(codesplit_source_file_tests unit/source_file_tests.cpp)
target_link_libraries(codesplit_source_file_tests PRIVATE codesplit_core)
add_test(NAME codesplit_source_file_tests COMMAND codesplit_source_file_tests)

add_executable(codesplit_cmake_integration_tests unit/cmake_integration_tests.cpp)
target_link_libraries(codesplit_cmake_integration_tests PRIVATE codesplit_core)
add_test(NAME codesplit_cmake_integration_tests COMMAND codesplit_cmake_integration_tests)

add_executable(codesplit_move_plan_tests unit/move_plan_tests.cpp)
target_link_libraries(codesplit_move_plan_tests PRIVATE codesplit_core)
add_test(NAME codesplit_move_plan_tests COMMAND codesplit_move_plan_tests)

add_executable(codesplit_move_dry_run_tests unit/move_dry_run_tests.cpp)
target_link_libraries(codesplit_move_dry_run_tests PRIVATE codesplit_core)
add_test(NAME codesplit_move_dry_run_tests COMMAND codesplit_move_dry_run_tests)

add_executable(codesplit_move_apply_tests unit/move_apply_tests.cpp)
target_link_libraries(codesplit_move_apply_tests PRIVATE codesplit_core)
add_test(NAME codesplit_move_apply_tests COMMAND codesplit_move_apply_tests)

add_executable(codesplit_text_report_tests unit/text_report_tests.cpp)
target_link_libraries(codesplit_text_report_tests PRIVATE codesplit_core)
add_test(NAME codesplit_text_report_tests COMMAND codesplit_text_report_tests)

add_executable(codesplit_json_report_tests unit/json_report_tests.cpp)
target_link_libraries(codesplit_json_report_tests PRIVATE codesplit_core)
add_test(NAME codesplit_json_report_tests COMMAND codesplit_json_report_tests)

if(CODESPLIT_HAS_CLANG_TOOLING)
    add_executable(codesplit_compilation_database_tests unit/compilation_database_tests.cpp)
    target_link_libraries(codesplit_compilation_database_tests PRIVATE codesplit_clang)
    add_test(NAME codesplit_compilation_database_tests COMMAND codesplit_compilation_database_tests)

    add_executable(codesplit_callable_inventory_tests unit/callable_inventory_tests.cpp)
    target_link_libraries(codesplit_callable_inventory_tests PRIVATE codesplit_clang)
    add_test(NAME codesplit_callable_inventory_tests COMMAND codesplit_callable_inventory_tests)

    add_test(
        NAME codesplit_mvp_e2e
        COMMAND ${CMAKE_COMMAND}
            -DCODESPLIT_EXECUTABLE=$<TARGET_FILE:codesplit>
            -DEXAMPLE_SOURCE=${PROJECT_SOURCE_DIR}/examples/mvp
            -DE2E_ROOT=${CMAKE_CURRENT_BINARY_DIR}/mvp-e2e
            -DTEST_GENERATOR=${CMAKE_GENERATOR}
            -DTEST_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            -DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DTEST_RC_COMPILER=${CMAKE_RC_COMPILER}
            -DTEST_MT=${CMAKE_MT}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/e2e/mvp_move.cmake
    )
else()
    add_executable(codesplit_compilation_database_tests
        unit/unavailable_compilation_database_tests.cpp
    )
    target_link_libraries(codesplit_compilation_database_tests PRIVATE codesplit_clang)
    add_test(NAME codesplit_compilation_database_tests COMMAND codesplit_compilation_database_tests)

    add_executable(codesplit_callable_inventory_tests
        unit/unavailable_callable_inventory_tests.cpp
    )
    target_link_libraries(codesplit_callable_inventory_tests PRIVATE codesplit_clang)
    add_test(NAME codesplit_callable_inventory_tests COMMAND codesplit_callable_inventory_tests)
endif()

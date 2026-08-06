add_executable(codesplit_unit_tests unit/command_line_tests.cpp)
target_link_libraries(codesplit_unit_tests PRIVATE codesplit_core)
add_test(NAME codesplit_unit_tests COMMAND codesplit_unit_tests)

add_executable(codesplit_source_file_tests unit/source_file_tests.cpp)
target_link_libraries(codesplit_source_file_tests PRIVATE codesplit_core)
add_test(NAME codesplit_source_file_tests COMMAND codesplit_source_file_tests)

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
else()
    add_executable(codesplit_compilation_database_tests
        unit/unavailable_compilation_database_tests.cpp
    )
    target_link_libraries(codesplit_compilation_database_tests PRIVATE codesplit_clang)
    add_test(NAME codesplit_compilation_database_tests COMMAND codesplit_compilation_database_tests)
endif()

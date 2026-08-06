#include "codesplit/reporting/json_report.hpp"

#include <iostream>
#include <string>

namespace {

int failure_count = 0;

void expect_equal(const std::string& actual, const std::string& expected,
                  const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAILED: " << message << '\n';
        std::cerr << "Expected:\n" << expected << "Actual:\n" << actual;
        ++failure_count;
    }
}

void formats_source_file_as_json() {
    const codesplit::analysis::SourceFileInfo info{
        .path = "src/large.cpp",
        .size_bytes = 153600,
        .line_count = 2048,
        .exceeds_size_limit = true,
    };
    const codesplit::analysis::CallableInventoryResult inventory{
        .compilation = {.command = {.working_directory = "build"}},
        .callables =
            {
                {
                    .kind = codesplit::analysis::CallableKind::free_function,
                    .qualified_name = "sample::helper",
                    .begin_offset = 20,
                    .end_offset = 140,
                    .size_bytes = 120,
                    .begin_line = 3,
                    .end_line = 5,
                    .constraints = {codesplit::analysis::CallableConstraint::exceeds_size_limit},
                },
            },
    };

    const auto report = codesplit::reporting::format_json_report(info, 100, inventory);

    expect_equal(report,
                 "{\n"
                 "  \"file\": \"src/large.cpp\",\n"
                 "  \"size_bytes\": 153600,\n"
                 "  \"line_count\": 2048,\n"
                 "  \"size_limit_kib\": 100,\n"
                 "  \"exceeds_size_limit\": true,\n"
                 "  \"compilation_command\": {\n"
                 "    \"available\": true,\n"
                 "    \"working_directory\": \"build\",\n"
                 "    \"error\": null\n"
                 "  },\n"
                 "  \"callable_inventory\": {\n"
                 "    \"available\": true,\n"
                 "    \"error\": null,\n"
                 "    \"definitions\": [\n"
                 "      {\n"
                 "        \"kind\": \"free_function\",\n"
                 "        \"qualified_name\": \"sample::helper\",\n"
                 "        \"begin_offset\": 20,\n"
                 "        \"end_offset\": 140,\n"
                 "        \"size_bytes\": 120,\n"
                 "        \"begin_line\": 3,\n"
                 "        \"end_line\": 5,\n"
                 "        \"constraints\": [\"exceeds_size_limit\"]\n"
                 "      }\n"
                 "    ]\n"
                 "  }\n"
                 "}\n",
                 "JSON report should contain stable source-file information");
}

void escapes_special_characters_in_path() {
    const codesplit::analysis::SourceFileInfo info{
        .path = "folder/\"quoted\"\nfile.cpp",
        .size_bytes = 512,
        .line_count = 10,
        .exceeds_size_limit = false,
    };
    const codesplit::analysis::CallableInventoryResult inventory{
        .compilation = {.error = "missing \"database\""},
        .error = "missing \"database\"",
    };

    const auto report = codesplit::reporting::format_json_report(info, 1, inventory);

    expect_equal(report,
                 "{\n"
                 "  \"file\": \"folder/\\\"quoted\\\"\\nfile.cpp\",\n"
                 "  \"size_bytes\": 512,\n"
                 "  \"line_count\": 10,\n"
                 "  \"size_limit_kib\": 1,\n"
                 "  \"exceeds_size_limit\": false,\n"
                 "  \"compilation_command\": {\n"
                 "    \"available\": false,\n"
                 "    \"working_directory\": null,\n"
                 "    \"error\": \"missing \\\"database\\\"\"\n"
                 "  },\n"
                 "  \"callable_inventory\": {\n"
                 "    \"available\": false,\n"
                 "    \"error\": \"missing \\\"database\\\"\",\n"
                 "    \"definitions\": []\n"
                 "  }\n"
                 "}\n",
                 "JSON report should escape special path characters");
}

} // namespace

int main() {
    formats_source_file_as_json();
    escapes_special_characters_in_path();

    if (failure_count == 0) {
        std::cout << "All JSON-report tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

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
                    .symbol_id = "c:@N@sample@F@helper#I#",
                    .declaration =
                        codesplit::analysis::SourceRange{
                            .path = "include/helper.hpp",
                            .begin_offset = 10,
                            .end_offset = 26,
                            .begin_line = 2,
                            .end_line = 2,
                        },
                    .begin_offset = 20,
                    .end_offset = 140,
                    .size_bytes = 120,
                    .begin_line = 3,
                    .end_line = 5,
                    .constraints = {codesplit::analysis::CallableConstraint::exceeds_size_limit},
                },
            },
        .dependencies =
            {
                {
                    .kind = codesplit::analysis::CallableDependencyKind::direct_call,
                    .source_symbol_id = "c:@N@sample@F@caller#I#",
                    .source_qualified_name = "sample::caller",
                    .target_symbol_id = "c:@N@sample@F@helper#I#",
                    .target_qualified_name = "sample::helper",
                },
                {
                    .kind = codesplit::analysis::CallableDependencyKind::type_reference,
                    .source_symbol_id = "c:@N@sample@F@caller#I#",
                    .source_qualified_name = "sample::caller",
                    .target_symbol_id = "c:@N@sample@S@Payload",
                    .target_qualified_name = "sample::Payload",
                },
                {
                    .kind = codesplit::analysis::CallableDependencyKind::global_read,
                    .source_symbol_id = "c:@N@sample@F@caller#I#",
                    .source_qualified_name = "sample::caller",
                    .target_symbol_id = "c:@N@sample@counter",
                    .target_qualified_name = "sample::counter",
                },
                {
                    .kind = codesplit::analysis::CallableDependencyKind::global_write,
                    .source_symbol_id = "c:@N@sample@F@caller#I#",
                    .source_qualified_name = "sample::caller",
                    .target_symbol_id = "c:@N@sample@counter",
                    .target_qualified_name = "sample::counter",
                },
            },
        .includes =
            {
                {
                    .kind = codesplit::analysis::IncludeKind::angled,
                    .written_name = "vector",
                    .resolved_path = "C:/toolchain/include/vector",
                    .origin =
                        {
                            .path = "src/large.cpp",
                            .begin_offset = 0,
                            .end_offset = 17,
                            .begin_line = 1,
                            .end_line = 1,
                        },
                },
            },
        .diagnostics =
            {
                {
                    .severity = codesplit::analysis::FrontendDiagnosticSeverity::warning,
                    .message = "unused parameter 'value'",
                    .path = "src/large.cpp",
                    .line = 3,
                    .column = 12,
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
                 "    \"diagnostics\": [\n"
                 "      {\n"
                 "        \"severity\": \"warning\",\n"
                 "        \"message\": \"unused parameter 'value'\",\n"
                 "        \"path\": \"src/large.cpp\",\n"
                 "        \"line\": 3,\n"
                 "        \"column\": 12\n"
                 "      }\n"
                 "    ],\n"
                 "    \"dependencies\": [\n"
                 "      {\n"
                 "        \"kind\": \"direct_call\",\n"
                 "        \"source_symbol_id\": \"c:@N@sample@F@caller#I#\",\n"
                 "        \"source_qualified_name\": \"sample::caller\",\n"
                 "        \"target_symbol_id\": \"c:@N@sample@F@helper#I#\",\n"
                 "        \"target_qualified_name\": \"sample::helper\"\n"
                 "      },\n"
                 "      {\n"
                 "        \"kind\": \"type_reference\",\n"
                 "        \"source_symbol_id\": \"c:@N@sample@F@caller#I#\",\n"
                 "        \"source_qualified_name\": \"sample::caller\",\n"
                 "        \"target_symbol_id\": \"c:@N@sample@S@Payload\",\n"
                 "        \"target_qualified_name\": \"sample::Payload\"\n"
                 "      },\n"
                 "      {\n"
                 "        \"kind\": \"global_read\",\n"
                 "        \"source_symbol_id\": \"c:@N@sample@F@caller#I#\",\n"
                 "        \"source_qualified_name\": \"sample::caller\",\n"
                 "        \"target_symbol_id\": \"c:@N@sample@counter\",\n"
                 "        \"target_qualified_name\": \"sample::counter\"\n"
                 "      },\n"
                 "      {\n"
                 "        \"kind\": \"global_write\",\n"
                 "        \"source_symbol_id\": \"c:@N@sample@F@caller#I#\",\n"
                 "        \"source_qualified_name\": \"sample::caller\",\n"
                 "        \"target_symbol_id\": \"c:@N@sample@counter\",\n"
                 "        \"target_qualified_name\": \"sample::counter\"\n"
                 "      }\n"
                 "    ],\n"
                 "    \"includes\": [\n"
                 "      {\n"
                 "        \"kind\": \"angled\",\n"
                 "        \"written_name\": \"vector\",\n"
                 "        \"resolved_path\": \"C:/toolchain/include/vector\",\n"
                 "        \"origin\": {\n"
                 "          \"path\": \"src/large.cpp\",\n"
                 "          \"begin_offset\": 0,\n"
                 "          \"end_offset\": 17,\n"
                 "          \"begin_line\": 1,\n"
                 "          \"end_line\": 1\n"
                 "        }\n"
                 "      }\n"
                 "    ],\n"
                 "    \"definitions\": [\n"
                 "      {\n"
                 "        \"kind\": \"free_function\",\n"
                 "        \"qualified_name\": \"sample::helper\",\n"
                 "        \"symbol_id\": \"c:@N@sample@F@helper#I#\",\n"
                 "        \"declaration\": {\n"
                 "          \"path\": \"include/helper.hpp\",\n"
                 "          \"begin_offset\": 10,\n"
                 "          \"end_offset\": 26,\n"
                 "          \"begin_line\": 2,\n"
                 "          \"end_line\": 2\n"
                 "        },\n"
                 "        \"owning_record\": null,\n"
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
                 "    \"diagnostics\": [],\n"
                 "    \"dependencies\": [],\n"
                 "    \"includes\": [],\n"
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

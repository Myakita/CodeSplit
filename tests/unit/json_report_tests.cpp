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
                    .linkage = codesplit::analysis::SymbolLinkage::unique_external,
                    .in_anonymous_namespace = true,
                    .qualified_name = "sample::helper",
                    .symbol_id = "c:@N@sample@F@helper#I#",
                    .enclosing_namespaces = {"sample"},
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
        .macros =
            {
                {
                    .source_symbol_id = "c:@N@sample@F@helper#I#",
                    .source_qualified_name = "sample::helper",
                    .macro_name = "APPLY_OFFSET",
                    .definition =
                        codesplit::analysis::SourceRange{
                            .path = "include/macros.hpp",
                            .begin_offset = 12,
                            .end_offset = 47,
                            .begin_line = 2,
                            .end_line = 2,
                        },
                    .expansions =
                        {
                            {
                                .path = "src/large.cpp",
                                .begin_offset = 30,
                                .end_offset = 49,
                                .begin_line = 3,
                                .end_line = 3,
                            },
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
                 "    \"macros\": [\n"
                 "      {\n"
                 "        \"source_symbol_id\": \"c:@N@sample@F@helper#I#\",\n"
                 "        \"source_qualified_name\": \"sample::helper\",\n"
                 "        \"macro_name\": \"APPLY_OFFSET\",\n"
                 "        \"definition\": {\n"
                 "          \"path\": \"include/macros.hpp\",\n"
                 "          \"begin_offset\": 12,\n"
                 "          \"end_offset\": 47,\n"
                 "          \"begin_line\": 2,\n"
                 "          \"end_line\": 2\n"
                 "        },\n"
                 "        \"expansions\": [\n"
                 "          {\n"
                 "            \"path\": \"src/large.cpp\",\n"
                 "            \"begin_offset\": 30,\n"
                 "            \"end_offset\": 49,\n"
                 "            \"begin_line\": 3,\n"
                 "            \"end_line\": 3\n"
                 "          }\n"
                 "        ]\n"
                 "      }\n"
                 "    ],\n"
                 "    \"definitions\": [\n"
                 "      {\n"
                 "        \"kind\": \"free_function\",\n"
                 "        \"linkage\": \"unique_external\",\n"
                 "        \"in_anonymous_namespace\": true,\n"
                 "        \"qualified_name\": \"sample::helper\",\n"
                 "        \"symbol_id\": \"c:@N@sample@F@helper#I#\",\n"
                 "        \"enclosing_namespaces\": [\"sample\"],\n"
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
                 "    \"macros\": [],\n"
                 "    \"definitions\": []\n"
                 "  }\n"
                 "}\n",
                 "JSON report should escape special path characters");
}

void formats_blocked_move_plan() {
    const codesplit::planning::MovePlan plan{
        .source_path = "src/large.cpp",
        .target_path = "src/new.cpp",
        .symbol_id = "c:@F@dependent#",
        .qualified_name = "dependent",
        .definition =
            codesplit::analysis::SourceRange{
                .path = "src/large.cpp",
                .begin_offset = 20,
                .end_offset = 80,
                .begin_line = 3,
                .end_line = 5,
            },
        .blockers =
            {
                {
                    .kind = codesplit::planning::MovePlanBlockerKind::outgoing_dependency,
                    .detail = "direct_call: helper",
                },
                {
                    .kind = codesplit::planning::MovePlanBlockerKind::macro_dependency,
                    .detail = "APPLY_OFFSET",
                },
            },
    };

    expect_equal(codesplit::reporting::format_json_move_plan(plan),
                 "{\n"
                 "  \"status\": \"blocked\",\n"
                 "  \"read_only\": true,\n"
                 "  \"source\": \"src/large.cpp\",\n"
                 "  \"target\": \"src/new.cpp\",\n"
                 "  \"symbol_id\": \"c:@F@dependent#\",\n"
                 "  \"qualified_name\": \"dependent\",\n"
                 "  \"enclosing_namespaces\": [],\n"
                 "  \"declaration_include\": null,\n"
                 "  \"definition\": {\n"
                 "    \"path\": \"src/large.cpp\",\n"
                 "    \"begin_offset\": 20,\n"
                 "    \"end_offset\": 80,\n"
                 "    \"begin_line\": 3,\n"
                 "    \"end_line\": 5\n"
                 "  },\n"
                 "  \"blockers\": [\n"
                 "    {\n"
                 "      \"kind\": \"outgoing_dependency\",\n"
                 "      \"detail\": \"direct_call: helper\"\n"
                 "    },\n"
                 "    {\n"
                 "      \"kind\": \"macro_dependency\",\n"
                 "      \"detail\": \"APPLY_OFFSET\"\n"
                 "    }\n"
                 "  ],\n"
                 "  \"steps\": []\n"
                 "}\n",
                 "JSON move plan should expose structured blockers");
}

void formats_blocked_move_dry_run() {
    const codesplit::planning::MoveDryRun dry_run{
        .plan = {.source_path = "src/large.cpp",
                 .target_path = "src/existing.cpp",
                 .symbol_id = "c:@F@isolated#"},
        .blockers = {{.kind = codesplit::planning::MoveDryRunBlockerKind::target_exists,
                      .detail = "src/existing.cpp"}},
    };

    expect_equal(codesplit::reporting::format_json_move_dry_run(dry_run),
                 "{\n"
                 "  \"status\": \"blocked\",\n"
                 "  \"read_only\": true,\n"
                 "  \"source\": \"src/large.cpp\",\n"
                 "  \"target\": \"src/existing.cpp\",\n"
                 "  \"symbol_id\": \"c:@F@isolated#\",\n"
                 "  \"blockers\": [\n"
                 "    {\"kind\": \"target_exists\", \"detail\": \"src/existing.cpp\"}\n"
                 "  ],\n"
                 "  \"replacements\": []\n"
                 "}\n",
                 "JSON dry run should expose a structured blocker");
}

void formats_blocked_move_apply() {
    const codesplit::planning::MoveApplyResult result{
        .dry_run = {.plan = {.source_path = "src/large.cpp",
                             .target_path = "src/isolated.cpp",
                             .symbol_id = "c:@F@isolated#"}},
        .blockers = {{.kind = codesplit::planning::MoveApplyBlockerKind::source_changed,
                      .detail = "source range no longer matches"}},
    };

    expect_equal(
        codesplit::reporting::format_json_move_apply(result),
        "{\n"
        "  \"status\": \"blocked\",\n"
        "  \"applied\": false,\n"
        "  \"rolled_back\": false,\n"
        "  \"source\": \"src/large.cpp\",\n"
        "  \"target\": \"src/isolated.cpp\",\n"
        "  \"symbol_id\": \"c:@F@isolated#\",\n"
        "  \"blockers\": [\n"
        "    {\"kind\": \"source_changed\", \"detail\": \"source range no longer matches\"}\n"
        "  ],\n"
        "  \"warnings\": []\n"
        "}\n",
        "JSON apply report should expose transactional blockers");
}

} // namespace

int main() {
    formats_source_file_as_json();
    escapes_special_characters_in_path();
    formats_blocked_move_plan();
    formats_blocked_move_dry_run();
    formats_blocked_move_apply();

    if (failure_count == 0) {
        std::cout << "All JSON-report tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

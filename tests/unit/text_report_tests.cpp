#include "codesplit/reporting/text_report.hpp"

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

void formats_source_file_information() {
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
                    .linkage = codesplit::analysis::SymbolLinkage::external,
                    .qualified_name = "sample::helper",
                    .begin_offset = 20,
                    .end_offset = 140,
                    .size_bytes = 120,
                    .begin_line = 3,
                    .end_line = 5,
                    .constraints = {codesplit::analysis::CallableConstraint::exceeds_size_limit},
                },
                {
                    .kind = codesplit::analysis::CallableKind::method,
                    .linkage = codesplit::analysis::SymbolLinkage::internal,
                    .in_anonymous_namespace = true,
                    .qualified_name = "sample::Worker::run",
                    .symbol_id = "c:@N@sample@S@Worker@F@run#I#",
                    .declaration =
                        codesplit::analysis::SourceRange{
                            .path = "include/worker.hpp",
                            .begin_offset = 42,
                            .end_offset = 59,
                            .begin_line = 4,
                            .end_line = 4,
                        },
                    .owning_record =
                        codesplit::analysis::SourceRange{
                            .path = "include/worker.hpp",
                            .begin_offset = 19,
                            .end_offset = 80,
                            .begin_line = 2,
                            .end_line = 6,
                        },
                    .begin_offset = 160,
                    .end_offset = 220,
                    .size_bytes = 60,
                    .begin_line = 8,
                    .end_line = 10,
                },
            },
        .dependencies =
            {
                {
                    .kind = codesplit::analysis::CallableDependencyKind::direct_call,
                    .source_symbol_id = "c:@N@sample@S@Worker@F@run#I#",
                    .source_qualified_name = "sample::Worker::run",
                    .target_symbol_id = "c:@N@sample@F@helper#I#",
                    .target_qualified_name = "sample::helper",
                },
                {
                    .kind = codesplit::analysis::CallableDependencyKind::type_reference,
                    .source_symbol_id = "c:@N@sample@S@Worker@F@run#I#",
                    .source_qualified_name = "sample::Worker::run",
                    .target_symbol_id = "c:@N@sample@S@Payload",
                    .target_qualified_name = "sample::Payload",
                },
                {
                    .kind = codesplit::analysis::CallableDependencyKind::global_read,
                    .source_symbol_id = "c:@N@sample@S@Worker@F@run#I#",
                    .source_qualified_name = "sample::Worker::run",
                    .target_symbol_id = "c:@N@sample@counter",
                    .target_qualified_name = "sample::counter",
                },
                {
                    .kind = codesplit::analysis::CallableDependencyKind::global_write,
                    .source_symbol_id = "c:@N@sample@S@Worker@F@run#I#",
                    .source_qualified_name = "sample::Worker::run",
                    .target_symbol_id = "c:@N@sample@counter",
                    .target_qualified_name = "sample::counter",
                },
            },
        .includes =
            {
                {
                    .kind = codesplit::analysis::IncludeKind::quoted,
                    .written_name = "worker.hpp",
                    .resolved_path = "include/worker.hpp",
                    .origin =
                        {
                            .path = "src/large.cpp",
                            .begin_offset = 0,
                            .end_offset = 21,
                            .begin_line = 1,
                            .end_line = 1,
                        },
                },
            },
        .macros =
            {
                {
                    .source_symbol_id = "c:@N@sample@S@Worker@F@run#I#",
                    .source_qualified_name = "sample::Worker::run",
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
                                .begin_offset = 180,
                                .end_offset = 199,
                                .begin_line = 9,
                                .end_line = 9,
                            },
                            {
                                .path = "src/large.cpp",
                                .begin_offset = 205,
                                .end_offset = 224,
                                .begin_line = 10,
                                .end_line = 10,
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
                    .line = 8,
                    .column = 21,
                },
            },
    };

    const auto report = codesplit::reporting::format_text_report(info, 100, inventory);

    expect_equal(report,
                 "File: src/large.cpp\n"
                 "Size: 153600 bytes\n"
                 "Lines: 2048\n"
                 "Exceeds 100 KiB: yes\n"
                 "Compilation command: available\n"
                 "Callable inventory: available\n"
                 "Frontend diagnostics: 1\n"
                 "- warning src/large.cpp:8:21: unused parameter 'value'\n"
                 "Callable definitions: 2\n"
                 "- free function sample::helper: lines 3-5, 120 bytes"
                 " [exceeds_size_limit]\n"
                 "  Linkage: external\n"
                 "- method sample::Worker::run: lines 8-10, 60 bytes\n"
                 "  Linkage: internal\n"
                 "  Anonymous namespace: yes\n"
                 "  Symbol ID: c:@N@sample@S@Worker@F@run#I#\n"
                 "  Declaration: include/worker.hpp, lines 4-4\n"
                 "  Owning record: include/worker.hpp, lines 2-6\n"
                 "Callable dependencies: 4\n"
                 "- direct call sample::Worker::run -> sample::helper\n"
                 "- type reference sample::Worker::run -> sample::Payload\n"
                 "- global read sample::Worker::run -> sample::counter\n"
                 "- global write sample::Worker::run -> sample::counter\n"
                 "Include dependencies: 1\n"
                 "- quoted worker.hpp -> include/worker.hpp at src/large.cpp:1\n"
                 "Macro dependencies: 1\n"
                 "- sample::Worker::run -> APPLY_OFFSET\n"
                 "  Definition: include/macros.hpp, lines 2-2\n"
                 "  Expansion: src/large.cpp:9\n"
                 "  Expansion: src/large.cpp:10\n",
                 "text report should contain stable source-file information");
}

void formats_file_within_limit() {
    const codesplit::analysis::SourceFileInfo info{
        .path = "small.cpp",
        .size_bytes = 512,
        .line_count = 10,
        .exceeds_size_limit = false,
    };
    const codesplit::analysis::CallableInventoryResult inventory{
        .compilation = {.error = "compile_commands.json was not found"},
        .error = "compile_commands.json was not found",
    };

    const auto report = codesplit::reporting::format_text_report(info, 1, inventory);

    expect_equal(report,
                 "File: small.cpp\n"
                 "Size: 512 bytes\n"
                 "Lines: 10\n"
                 "Exceeds 1 KiB: no\n"
                 "Compilation command: unavailable\n"
                 "Reason: compile_commands.json was not found\n"
                 "Callable inventory: unavailable\n",
                 "text report should show a file within the limit");
}

void formats_ready_move_plan() {
    const codesplit::planning::MovePlan plan{
        .source_path = "src/large.cpp",
        .target_path = "src/isolated.cpp",
        .symbol_id = "c:@F@isolated#I#",
        .qualified_name = "isolated",
        .definition =
            codesplit::analysis::SourceRange{
                .path = "src/large.cpp",
                .begin_offset = 100,
                .end_offset = 160,
                .begin_line = 8,
                .end_line = 10,
            },
        .steps =
            {
                {.kind = codesplit::planning::MovePlanStepKind::create_implementation},
                {.kind = codesplit::planning::MovePlanStepKind::replace_body_with_delegate},
                {.kind = codesplit::planning::MovePlanStepKind::validate_frontend},
                {.kind = codesplit::planning::MovePlanStepKind::build_and_test},
            },
    };

    expect_equal(codesplit::reporting::format_text_move_plan(plan),
                 "Move plan: ready\n"
                 "Read-only: yes\n"
                 "Source: src/large.cpp\n"
                 "Target: src/isolated.cpp\n"
                 "Symbol ID: c:@F@isolated#I#\n"
                 "Callable: isolated\n"
                 "Definition: lines 8-10, offsets 100-160\n"
                 "Planned steps: 4\n"
                 "1. create extracted implementation in target\n"
                 "2. replace source body with a delegating call\n"
                 "3. repeat frontend analysis\n"
                 "4. build and test affected project\n",
                 "text move plan should expose ordered read-only steps");
}

void formats_move_dry_run() {
    const codesplit::planning::MoveDryRun dry_run{
        .plan = {.source_path = "src/large.cpp",
                 .target_path = "src/isolated.cpp",
                 .symbol_id = "c:@F@isolated#"},
        .replacements =
            {
                {.path = "src/large.cpp", .begin_offset = 10, .end_offset = 40},
                {.path = "src/isolated.cpp",
                 .begin_offset = 0,
                 .end_offset = 0,
                 .replacement_text = "int isolated() {}\n"},
            },
    };

    expect_equal(codesplit::reporting::format_text_move_dry_run(dry_run),
                 "Move dry run: ready\n"
                 "Read-only: yes\n"
                 "Source: src/large.cpp\n"
                 "Target: src/isolated.cpp\n"
                 "Symbol ID: c:@F@isolated#\n"
                 "Replacements: 2\n"
                 "- src/large.cpp: offsets 10-40, insert 0 bytes\n"
                 "- src/isolated.cpp: offsets 0-0, insert 18 bytes\n",
                 "text dry run should expose replacement ranges without applying them");
}

void formats_successful_move_apply() {
    const codesplit::planning::MoveApplyResult result{
        .dry_run = {.plan = {.source_path = "src/large.cpp",
                             .target_path = "src/isolated.cpp",
                             .symbol_id = "c:@F@isolated#"}},
        .applied = true,
    };

    expect_equal(codesplit::reporting::format_text_move_apply(result),
                 "Move apply: applied\n"
                 "Source: src/large.cpp\n"
                 "Target: src/isolated.cpp\n"
                 "Symbol ID: c:@F@isolated#\n"
                 "Validated: no\n"
                 "Rolled back: no\n",
                 "text apply report should confirm committed paths");
}

} // namespace

int main() {
    formats_source_file_information();
    formats_file_within_limit();
    formats_ready_move_plan();
    formats_move_dry_run();
    formats_successful_move_apply();

    if (failure_count == 0) {
        std::cout << "All text-report tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

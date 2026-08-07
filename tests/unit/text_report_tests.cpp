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
                 "- method sample::Worker::run: lines 8-10, 60 bytes\n"
                 "  Symbol ID: c:@N@sample@S@Worker@F@run#I#\n"
                 "  Declaration: include/worker.hpp, lines 4-4\n"
                 "  Owning record: include/worker.hpp, lines 2-6\n"
                 "Callable dependencies: 4\n"
                 "- direct call sample::Worker::run -> sample::helper\n"
                 "- type reference sample::Worker::run -> sample::Payload\n"
                 "- global read sample::Worker::run -> sample::counter\n"
                 "- global write sample::Worker::run -> sample::counter\n",
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

} // namespace

int main() {
    formats_source_file_information();
    formats_file_within_limit();

    if (failure_count == 0) {
        std::cout << "All text-report tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

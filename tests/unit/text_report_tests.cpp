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
    const codesplit::analysis::CompilationCommandResult compilation{
        .command = {.working_directory = "build"},
    };

    const auto report = codesplit::reporting::format_text_report(info, 100, compilation);

    expect_equal(report,
                 "File: src/large.cpp\n"
                 "Size: 153600 bytes\n"
                 "Lines: 2048\n"
                 "Exceeds 100 KiB: yes\n"
                 "Compilation command: available\n",
                 "text report should contain stable source-file information");
}

void formats_file_within_limit() {
    const codesplit::analysis::SourceFileInfo info{
        .path = "small.cpp",
        .size_bytes = 512,
        .line_count = 10,
        .exceeds_size_limit = false,
    };
    const codesplit::analysis::CompilationCommandResult compilation{
        .error = "compile_commands.json was not found",
    };

    const auto report = codesplit::reporting::format_text_report(info, 1, compilation);

    expect_equal(report,
                 "File: small.cpp\n"
                 "Size: 512 bytes\n"
                 "Lines: 10\n"
                 "Exceeds 1 KiB: no\n"
                 "Compilation command: unavailable\n"
                 "Reason: compile_commands.json was not found\n",
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

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

    const auto report = codesplit::reporting::format_json_report(info, 100);

    expect_equal(report,
                 "{\n"
                 "  \"file\": \"src/large.cpp\",\n"
                 "  \"size_bytes\": 153600,\n"
                 "  \"line_count\": 2048,\n"
                 "  \"size_limit_kib\": 100,\n"
                 "  \"exceeds_size_limit\": true\n"
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

    const auto report = codesplit::reporting::format_json_report(info, 1);

    expect_equal(report,
                 "{\n"
                 "  \"file\": \"folder/\\\"quoted\\\"\\nfile.cpp\",\n"
                 "  \"size_bytes\": 512,\n"
                 "  \"line_count\": 10,\n"
                 "  \"size_limit_kib\": 1,\n"
                 "  \"exceeds_size_limit\": false\n"
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

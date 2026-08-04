#include "codesplit/analysis/source_file.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failure_count = 0;

std::filesystem::path unique_source_path() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("codesplit_source_file_" + std::to_string(suffix) + ".cpp");
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

class TemporarySourceFile {
  public:
    explicit TemporarySourceFile(const std::string& content) : path_{unique_source_path()} {
        std::ofstream stream{path_, std::ios::binary};
        stream << content;
    }

    ~TemporarySourceFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporarySourceFile(const TemporarySourceFile&) = delete;
    TemporarySourceFile& operator=(const TemporarySourceFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  private:
    std::filesystem::path path_;
};

void analyzes_file_size_and_line_count() {
    const std::string content = "int first();\nint second();\n";
    const TemporarySourceFile source{content};

    const auto result = codesplit::analysis::analyze_source_file(source.path(), 10);

    expect(static_cast<bool>(result), "existing source file should be analyzed");
    expect(result.info.path == source.path(), "source path should be preserved");
    expect(result.info.size_bytes == content.size(), "file size should be measured in bytes");
    expect(result.info.line_count == 2, "newline-terminated file should have two lines");
    expect(result.info.exceeds_size_limit, "file should exceed the ten-byte limit");
}

void counts_last_line_without_newline() {
    const TemporarySourceFile source{"int first();\nint second();"};

    const auto result = codesplit::analysis::analyze_source_file(source.path());

    expect(static_cast<bool>(result), "source without trailing newline should be analyzed");
    expect(result.info.line_count == 2, "unterminated last line should be counted");
}

void rejects_missing_file() {
    const auto path = unique_source_path();
    const auto result = codesplit::analysis::analyze_source_file(path);

    expect(!result, "missing source file should be rejected");
    expect(!result.error.empty(), "missing source file should produce an error");
}

} // namespace

int main() {
    analyzes_file_size_and_line_count();
    counts_last_line_without_newline();
    rejects_missing_file();

    if (failure_count == 0) {
        std::cout << "All source-file tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

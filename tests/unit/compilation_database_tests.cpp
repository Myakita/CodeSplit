#include "codesplit/analysis/compilation_database.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failure_count = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto encoded = path.generic_u8string();
    return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

class TemporaryProject {
  public:
    TemporaryProject() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("codesplit_compilation_database_" + std::to_string(suffix));
        std::filesystem::create_directories(root_ / "build");

        std::ofstream source{source_path()};
        source << "int main() {}\n";
    }

    ~TemporaryProject() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    TemporaryProject(const TemporaryProject&) = delete;
    TemporaryProject& operator=(const TemporaryProject&) = delete;

    [[nodiscard]] std::filesystem::path source_path() const { return root_ / "main.cpp"; }
    [[nodiscard]] std::filesystem::path build_path() const { return root_ / "build"; }

    void write_compilation_database(const std::filesystem::path& source) const {
        std::ofstream database{build_path() / "compile_commands.json"};
        database << "[\n"
                    "  {\n"
                    "    \"directory\": \""
                 << path_to_utf8(root_) << "\",\n"
                 << "    \"file\": \"" << path_to_utf8(source) << "\",\n"
                 << "    \"arguments\": [\"cl\", \"/std:c++20\", \"/c\", \"" << path_to_utf8(source)
                 << "\"]\n"
                 << "  }\n"
                    "]\n";
    }

  private:
    std::filesystem::path root_;
};

void loads_command_for_source_file() {
    const TemporaryProject project;
    project.write_compilation_database("main.cpp");

    const auto result =
        codesplit::analysis::load_compilation_command(project.build_path(), project.source_path());

    expect(static_cast<bool>(result), "existing compilation command should be loaded");
    expect(result.command.working_directory == project.source_path().parent_path(),
           "working directory should be preserved");
    expect(std::ranges::find(result.command.arguments, "/std:c++20") !=
               result.command.arguments.end(),
           "language standard should be preserved");
    expect(std::ranges::find(result.command.arguments, "/c") != result.command.arguments.end(),
           "compile-only option should be preserved");
}

void rejects_missing_compilation_database() {
    const TemporaryProject project;

    const auto result =
        codesplit::analysis::load_compilation_command(project.build_path(), project.source_path());

    expect(!result, "missing compilation database should be rejected");
    expect(result.error.find("Unable to load compilation database") != std::string::npos,
           "error should identify the missing compilation database");
}

void rejects_source_without_command() {
    const TemporaryProject project;
    project.write_compilation_database(project.source_path().parent_path() / "other.cpp");

    const auto result =
        codesplit::analysis::load_compilation_command(project.build_path(), project.source_path());

    expect(!result, "source without compilation command should be rejected");
    expect(result.error.find("No compilation command found") != std::string::npos,
           "error should identify the missing compilation command");
}

} // namespace

int main() {
    loads_command_for_source_file();
    rejects_missing_compilation_database();
    rejects_source_without_command();

    if (failure_count == 0) {
        std::cout << "All compilation-database tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

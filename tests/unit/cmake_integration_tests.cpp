#include "codesplit/planning/cmake_integration.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

class TemporaryProject {
  public:
    TemporaryProject() {
        root_ = std::filesystem::temp_directory_path() / "codesplit-cmake-integration-tests";
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(build_path());
        std::ofstream{root_ / "CMakeLists.txt", std::ios::binary}
            << "add_library(core source.cpp)\n";
        std::ofstream{build_path() / "CMakeCache.txt", std::ios::binary}
            << "CMAKE_HOME_DIRECTORY:INTERNAL=" << root_.generic_string() << '\n';
    }

    ~TemporaryProject() { std::filesystem::remove_all(root_); }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }
    [[nodiscard]] std::filesystem::path build_path() const { return root_ / "build"; }

  private:
    std::filesystem::path root_;
};

void plans_explicit_target_sources_append() {
    TemporaryProject project;
    const codesplit::analysis::CompilationCommand command{
        .working_directory = project.build_path(),
        .arguments = {"cl.exe", "/FoCMakeFiles\\core.dir\\source.cpp.obj", "/c",
                      (project.root() / "source.cpp").string()},
    };

    const auto integration = codesplit::planning::plan_cmake_integration(
        project.build_path(), command, project.root() / "isolated.cpp");

    if (!integration) {
        std::cerr << "Integration error: " << integration.error << '\n';
    }
    expect(static_cast<bool>(integration), "CMake metadata should produce an integration plan");
    expect(integration.target_name == "core", "object path should identify the CMake target");
    expect(integration.replacement.path == project.root() / "CMakeLists.txt",
           "replacement should update the root CMake file");
    expect(integration.replacement.begin_offset == 0,
           "CMake integration should compare the complete build file");
    expect(integration.replacement.expected_text == "add_library(core source.cpp)\n",
           "replacement should guard against concurrent build-file changes");
    expect(integration.replacement.replacement_text ==
               "add_library(core source.cpp)\n\ntarget_sources(core PRIVATE \"isolated.cpp\")\n",
           "integration should add a declarative target_sources call");
}

void rejects_unknown_build_target() {
    TemporaryProject project;
    const codesplit::analysis::CompilationCommand command{
        .working_directory = project.build_path(),
        .arguments = {"clang++", "-c", (project.root() / "source.cpp").string()},
    };

    const auto integration = codesplit::planning::plan_cmake_integration(
        project.build_path(), command, project.root() / "isolated.cpp");

    expect(!integration, "command without CMake object path should be rejected");
    expect(integration.error.find("does not identify") != std::string::npos,
           "error should explain the missing build target identity");
}

} // namespace

int main() {
    plans_explicit_target_sources_append();
    rejects_unknown_build_target();
}

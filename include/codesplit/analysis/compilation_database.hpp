#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace codesplit::analysis {

struct CompilationCommand {
    std::filesystem::path working_directory;
    std::vector<std::string> arguments;
};

struct CompilationCommandResult {
    CompilationCommand command;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] CompilationCommandResult
load_compilation_command(const std::filesystem::path& build_path,
                         const std::filesystem::path& source_path);

} // namespace codesplit::analysis

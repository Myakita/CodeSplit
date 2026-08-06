#pragma once

#include <clang/Tooling/CompilationDatabase.h>

#include <filesystem>
#include <string>

namespace codesplit::analysis::detail {

struct CompilationCommandLookup {
    clang::tooling::CompileCommand command;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] CompilationCommandLookup
find_compilation_command(const std::filesystem::path& build_path,
                         const std::filesystem::path& source_path);

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);

} // namespace codesplit::analysis::detail

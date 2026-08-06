#include "codesplit/analysis/compilation_database.hpp"

#include <clang/Tooling/CompilationDatabase.h>

#include <memory>
#include <system_error>

namespace codesplit::analysis {
namespace {

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto encoded = path.generic_u8string();
    return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::filesystem::path absolute_normalized(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical_path = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical_path;
    }

    error.clear();
    const auto absolute_path = std::filesystem::absolute(path, error);
    return error ? path.lexically_normal() : absolute_path.lexically_normal();
}

std::filesystem::path source_path_for(const clang::tooling::CompileCommand& command) {
    const std::filesystem::path file{command.Filename};
    if (file.is_absolute()) {
        return absolute_normalized(file);
    }

    return absolute_normalized(std::filesystem::path{command.Directory} / file);
}

} // namespace

CompilationCommandResult load_compilation_command(const std::filesystem::path& build_path,
                                                  const std::filesystem::path& source_path) {
    CompilationCommandResult result;
    std::string database_error;
    auto database = clang::tooling::CompilationDatabase::loadFromDirectory(path_to_utf8(build_path),
                                                                           database_error);
    if (!database) {
        result.error = "Unable to load compilation database from " + path_to_utf8(build_path) +
                       ": " + database_error;
        return result;
    }

    const auto normalized_source_path = absolute_normalized(source_path);
    for (const auto& command : database->getAllCompileCommands()) {
        if (source_path_for(command) != normalized_source_path) {
            continue;
        }

        result.command.working_directory = std::filesystem::path{command.Directory};
        result.command.arguments = command.CommandLine;
        return result;
    }

    result.error = "No compilation command found for: " + path_to_utf8(source_path);
    return result;
}

} // namespace codesplit::analysis

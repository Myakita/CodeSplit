#pragma once

#include "codesplit/analysis/compilation_database.hpp"
#include "codesplit/planning/move_dry_run.hpp"

#include <filesystem>
#include <string>

namespace codesplit::planning {

struct CMakeIntegrationResult {
    std::filesystem::path project_root;
    std::filesystem::path cmake_file;
    std::string target_name;
    TextReplacement replacement;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] CMakeIntegrationResult
plan_cmake_integration(const std::filesystem::path& build_path,
                       const analysis::CompilationCommand& command,
                       const std::filesystem::path& target_source_path);

void add_cmake_integration(MoveDryRun& dry_run, const CMakeIntegrationResult& integration,
                           const std::filesystem::path& build_path);

} // namespace codesplit::planning

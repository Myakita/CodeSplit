#include "codesplit/planning/cmake_integration.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <string_view>

namespace codesplit::planning {
namespace {

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string cmake_value(const std::string& cache, std::string_view key) {
    const auto prefix = std::string{key} + ":INTERNAL=";
    const auto begin = cache.find(prefix);
    if (begin == std::string::npos) {
        return {};
    }
    const auto value_begin = begin + prefix.size();
    const auto end = cache.find_first_of("\r\n", value_begin);
    return cache.substr(value_begin, end - value_begin);
}

std::string target_name_for(const analysis::CompilationCommand& command) {
    const std::regex object_path{R"(CMakeFiles[\\/]([A-Za-z0-9_.+-]+)\.dir[\\/])"};
    std::smatch match;
    for (const auto& argument : command.arguments) {
        if (std::regex_search(argument, match, object_path)) {
            return match[1].str();
        }
    }
    return {};
}

std::string quoted_cmake_path(const std::filesystem::path& path) {
    auto value = path.generic_string();
    std::string escaped;
    escaped.reserve(value.size());
    for (const auto character : value) {
        if (character == '"' || character == '\\') {
            escaped += '\\';
        }
        escaped += character;
    }
    return escaped;
}

} // namespace

CMakeIntegrationResult plan_cmake_integration(const std::filesystem::path& build_path,
                                              const analysis::CompilationCommand& command,
                                              const std::filesystem::path& target_source_path) {
    CMakeIntegrationResult result;
    const auto cache = read_file(build_path / "CMakeCache.txt");
    if (!cache.has_value()) {
        result.error = "CMakeCache.txt is unavailable in the build path";
        return result;
    }
    result.project_root = cmake_value(*cache, "CMAKE_HOME_DIRECTORY");
    if (result.project_root.empty()) {
        result.error = "CMAKE_HOME_DIRECTORY is unavailable in CMakeCache.txt";
        return result;
    }
    result.target_name = target_name_for(command);
    if (result.target_name.empty()) {
        result.error = "compilation command does not identify a CMake target";
        return result;
    }
    result.cmake_file = result.project_root / "CMakeLists.txt";
    const auto cmake_text = read_file(result.cmake_file);
    if (!cmake_text.has_value()) {
        result.error = "root CMakeLists.txt is unavailable";
        return result;
    }
    std::error_code error;
    const auto absolute_target = std::filesystem::absolute(target_source_path, error);
    if (error) {
        result.error = "target source path cannot be resolved";
        return result;
    }
    const auto absolute_root = std::filesystem::absolute(result.project_root, error);
    const auto relative_target =
        absolute_target.lexically_normal().lexically_relative(absolute_root.lexically_normal());
    if (error || relative_target.empty() || *relative_target.begin() == "..") {
        result.error = "target source is outside the CMake project root";
        return result;
    }
    const auto separator = cmake_text->empty() || cmake_text->ends_with('\n') ? "" : "\n";
    const auto addition = std::string{separator} + "\ntarget_sources(" + result.target_name +
                          " PRIVATE \"" + quoted_cmake_path(relative_target) + "\")\n";
    result.replacement = {
        .path = result.cmake_file,
        .begin_offset = 0,
        .end_offset = cmake_text->size(),
        .expected_text = *cmake_text,
        .replacement_text = *cmake_text + addition,
    };
    return result;
}

void add_cmake_integration(MoveDryRun& dry_run, const CMakeIntegrationResult& integration,
                           const std::filesystem::path& build_path) {
    if (!integration) {
        dry_run.blockers.push_back({
            .kind = MoveDryRunBlockerKind::build_integration_unavailable,
            .detail = integration.error,
        });
        dry_run.replacements.clear();
        return;
    }
    dry_run.replacements.push_back(integration.replacement);
    dry_run.build_path = build_path;
    dry_run.project_root = integration.project_root;
    dry_run.build_target = integration.target_name;
}

} // namespace codesplit::planning

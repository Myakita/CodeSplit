#include "codesplit/analysis/callable_inventory.hpp"
#include "codesplit/analysis/source_file.hpp"
#include "codesplit/cli/command_line.hpp"
#include "codesplit/planning/cmake_integration.hpp"
#include "codesplit/planning/move_apply.hpp"
#include "codesplit/planning/move_dry_run.hpp"
#include "codesplit/planning/move_plan.hpp"
#include "codesplit/reporting/json_report.hpp"
#include "codesplit/reporting/text_report.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::string shell_quote(const std::string& value) {
#ifdef _WIN32
    if (value.find_first_of("\"&|<>^") != std::string::npos) {
        return {};
    }
    return '"' + value + '"';
#else
    std::string quoted{"'"};
    for (const auto character : value) {
        quoted += character == '\'' ? "'\\''" : std::string(1, character);
    }
    return quoted + '\'';
#endif
}

int run_shell_command(std::string command) {
#ifdef _WIN32
    command += " >NUL 2>&1";
    command = '"' + command + '"';
#else
    command += " >/dev/null 2>&1";
#endif
    return std::system(command.c_str());
}

codesplit::planning::MoveValidationResult validate_build(const std::filesystem::path& build_path,
                                                         const std::string& target_name) {
    const auto quoted_cmake = shell_quote(CODESPLIT_CMAKE_COMMAND);
    const auto quoted_build = shell_quote(build_path.string());
    if (quoted_cmake.empty() || quoted_build.empty() || target_name.empty()) {
        return {.detail = "build command contains unsupported shell characters"};
    }
    const auto command = quoted_cmake + " --build " + quoted_build + " --target " + target_name;
    const auto exit_code = run_shell_command(command);
    return {
        .success = exit_code == 0,
        .detail = exit_code == 0 ? std::string{} : "CMake target build failed",
    };
}

codesplit::planning::MoveValidationResult
restore_cmake_build_graph(const std::filesystem::path& project_root,
                          const std::filesystem::path& build_path) {
    const auto quoted_cmake = shell_quote(CODESPLIT_CMAKE_COMMAND);
    const auto quoted_source = shell_quote(project_root.string());
    const auto quoted_build = shell_quote(build_path.string());
    if (quoted_cmake.empty() || quoted_source.empty() || quoted_build.empty()) {
        return {.detail = "CMake configure command contains unsupported shell characters"};
    }
    const auto command = quoted_cmake + " -S " + quoted_source + " -B " + quoted_build;
    const auto exit_code = run_shell_command(command);
    return {
        .success = exit_code == 0,
        .detail = exit_code == 0 ? std::string{} : "CMake build graph restore failed",
    };
}

} // namespace

int main(int argc, char* argv[]) {
    const auto command = codesplit::cli::parse_command_line(argc, argv);

    if (!command) {
        std::cerr << command.error << '\n';
        std::cerr << codesplit::cli::usage();
        return 2;
    }

    if (command.show_help) {
        std::cout << codesplit::cli::usage();
        return 0;
    }

    if (command.show_version) {
        std::cout << "CodeSplit " << CODESPLIT_VERSION << '\n';
        return 0;
    }

    constexpr std::uintmax_t bytes_per_kib = 1024U;
    const auto size_limit_bytes = command.max_size_kib * bytes_per_kib;
    const auto analysis =
        codesplit::analysis::analyze_source_file(command.input_path, size_limit_bytes);
    if (!analysis) {
        std::cerr << analysis.error << '\n';
        return 1;
    }

    const auto inventory = codesplit::analysis::inventory_callables(
        command.build_path, command.input_path, size_limit_bytes);

    if (command.operation == codesplit::cli::Operation::plan_move ||
        command.operation == codesplit::cli::Operation::dry_run_move ||
        command.operation == codesplit::cli::Operation::apply_move) {
        const auto plan = codesplit::planning::plan_callable_move(
            command.input_path, command.target_path, command.symbol_id, inventory);
        if (command.operation == codesplit::cli::Operation::dry_run_move ||
            command.operation == codesplit::cli::Operation::apply_move) {
            auto dry_run = codesplit::planning::draft_callable_move(plan);
            if (dry_run) {
                const auto integration = codesplit::planning::plan_cmake_integration(
                    command.build_path, inventory.compilation.command, command.target_path);
                codesplit::planning::add_cmake_integration(dry_run, integration,
                                                           command.build_path);
            }
            if (command.operation == codesplit::cli::Operation::apply_move) {
                const auto validator = [&](const std::filesystem::path& source_path,
                                           const std::filesystem::path& target_path) {
                    const auto validation_failure = [](std::string_view label,
                                                       const auto& validation) {
                        auto detail =
                            std::string{label} + " validation failed: " + validation.error;
                        const auto diagnostic =
                            std::ranges::find_if(validation.diagnostics, [](const auto& candidate) {
                                using codesplit::analysis::FrontendDiagnosticSeverity;
                                return candidate.severity == FrontendDiagnosticSeverity::error ||
                                       candidate.severity == FrontendDiagnosticSeverity::fatal;
                            });
                        if (diagnostic != validation.diagnostics.end()) {
                            detail += ": " + diagnostic->message;
                        }
                        return codesplit::planning::MoveValidationResult{.detail =
                                                                             std::move(detail)};
                    };
                    const auto validated_source = codesplit::analysis::inventory_callables(
                        inventory.compilation.command, command.input_path, source_path,
                        size_limit_bytes);
                    if (!validated_source) {
                        return validation_failure("source", validated_source);
                    }
                    const auto validated_target = codesplit::analysis::inventory_callables(
                        inventory.compilation.command, command.input_path, target_path,
                        size_limit_bytes);
                    if (!validated_target) {
                        return validation_failure("target", validated_target);
                    }
                    return validate_build(dry_run.build_path, dry_run.build_target);
                };
                const auto rollback_action = [&] {
                    return restore_cmake_build_graph(dry_run.project_root, dry_run.build_path);
                };
                const auto result =
                    codesplit::planning::apply_callable_move(dry_run, validator, rollback_action);
                if (command.report_format == codesplit::cli::ReportFormat::json) {
                    std::cout << codesplit::reporting::format_json_move_apply(result);
                } else {
                    std::cout << codesplit::reporting::format_text_move_apply(result);
                }
                if (result) {
                    return 0;
                }
                return dry_run ? 4 : 3;
            }
            if (command.report_format == codesplit::cli::ReportFormat::json) {
                std::cout << codesplit::reporting::format_json_move_dry_run(dry_run);
            } else {
                std::cout << codesplit::reporting::format_text_move_dry_run(dry_run);
            }
            return dry_run ? 0 : 3;
        }
        if (command.report_format == codesplit::cli::ReportFormat::json) {
            std::cout << codesplit::reporting::format_json_move_plan(plan);
        } else {
            std::cout << codesplit::reporting::format_text_move_plan(plan);
        }
        return plan ? 0 : 3;
    }

    if (command.report_format == codesplit::cli::ReportFormat::json) {
        std::cout << codesplit::reporting::format_json_report(analysis.info, command.max_size_kib,
                                                              inventory);
    } else {
        std::cout << codesplit::reporting::format_text_report(analysis.info, command.max_size_kib,
                                                              inventory);
    }

    return 0;
}

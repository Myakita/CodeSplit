#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace codesplit::cli {

enum class Operation {
    analyze,
    plan_move,
    dry_run_move,
    apply_move,
};

enum class ReportFormat {
    text,
    json,
};

enum class ExitCode : int {
    success = 0,
    runtime_error = 1,
    invalid_command = 2,
    transformation_blocked = 3,
    apply_failed = 4,
};

[[nodiscard]] constexpr int process_exit_code(ExitCode exit_code) noexcept {
    return static_cast<int>(exit_code);
}

struct CommandLine {
    Operation operation{Operation::analyze};
    std::filesystem::path input_path;
    std::filesystem::path target_path;
    std::filesystem::path build_path{"build"};
    std::string symbol_id;
    std::uintmax_t max_size_kib{100};
    ReportFormat report_format{ReportFormat::text};
    bool show_help{false};
    bool show_version{false};
    bool confirm_apply{false};
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] CommandLine parse_command_line(int argc, char* argv[]);
[[nodiscard]] std::string usage();

} // namespace codesplit::cli

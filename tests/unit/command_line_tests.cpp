#include "codesplit/cli/command_line.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int failure_count = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

codesplit::cli::CommandLine parse(std::vector<std::string> arguments) {
    std::vector<char*> raw_arguments;
    raw_arguments.reserve(arguments.size());
    for (auto& argument : arguments) {
        raw_arguments.push_back(argument.data());
    }

    return codesplit::cli::parse_command_line(static_cast<int>(raw_arguments.size()),
                                              raw_arguments.data());
}

void parses_analyze_command() {
    const auto command = parse({"codesplit", "analyze", "src/large.cpp"});

    expect(static_cast<bool>(command), "analyze command should be valid");
    expect(command.operation == codesplit::cli::Operation::analyze, "operation should be analyze");
    expect(command.input_path == "src/large.cpp", "input path should be preserved");
    expect(command.build_path == "build", "default build path should be build");
    expect(command.max_size_kib == 100, "default size limit should be 100 KiB");
    expect(command.report_format == codesplit::cli::ReportFormat::text,
           "default report format should be text");
}

void parses_plan_move_command() {
    const auto command =
        parse({"codesplit", "plan-move", "src/large.cpp", "--symbol-id", "c:@F@isolated#I#",
               "--target", "src/isolated.cpp", "--build-path", "out", "--format", "json"});

    expect(static_cast<bool>(command), "plan-move command should be valid");
    expect(command.operation == codesplit::cli::Operation::plan_move,
           "operation should be plan-move");
    expect(command.symbol_id == "c:@F@isolated#I#", "symbol ID should be preserved");
    expect(command.target_path == "src/isolated.cpp", "target path should be preserved");
    expect(command.build_path == "out", "plan should preserve build path");
    expect(command.report_format == codesplit::cli::ReportFormat::json,
           "plan should preserve report format");
}

void rejects_incomplete_plan_move_command() {
    const auto missing_symbol =
        parse({"codesplit", "plan-move", "src/large.cpp", "--target", "src/new.cpp"});
    const auto missing_target =
        parse({"codesplit", "plan-move", "src/large.cpp", "--symbol-id", "c:@F@isolated#I#"});

    expect(!missing_symbol, "plan without symbol ID should be rejected");
    expect(missing_symbol.error == "Missing required --symbol-id for plan-move.",
           "missing symbol error should be explicit");
    expect(!missing_target, "plan without target should be rejected");
    expect(missing_target.error == "Missing required --target for plan-move.",
           "missing target error should be explicit");
}

void parses_dry_run_move_command() {
    const auto command = parse({"codesplit", "dry-run-move", "src/large.cpp", "--symbol-id",
                                "c:@F@isolated#", "--target", "src/isolated.cpp"});

    expect(static_cast<bool>(command), "dry-run-move command should be valid");
    expect(command.operation == codesplit::cli::Operation::dry_run_move,
           "operation should be dry-run-move");
    expect(command.symbol_id == "c:@F@isolated#", "dry run should preserve the symbol ID");
    expect(command.target_path == "src/isolated.cpp", "dry run should preserve target path");
}

void parses_confirmed_apply_move_command() {
    const auto command = parse({"codesplit", "apply-move", "src/large.cpp", "--symbol-id",
                                "c:@F@isolated#", "--target", "src/isolated.cpp", "--confirm"});
    const auto unconfirmed = parse({"codesplit", "apply-move", "src/large.cpp", "--symbol-id",
                                    "c:@F@isolated#", "--target", "src/isolated.cpp"});

    expect(static_cast<bool>(command), "confirmed apply-move command should be valid");
    expect(command.operation == codesplit::cli::Operation::apply_move,
           "operation should be apply-move");
    expect(command.confirm_apply, "apply command should preserve explicit confirmation");
    expect(!unconfirmed, "unconfirmed apply command should be rejected");
    expect(unconfirmed.error == "Missing required --confirm for apply-move.",
           "unconfirmed apply error should be explicit");
}

void parses_version_option() {
    const auto command = parse({"codesplit", "--version"});

    expect(static_cast<bool>(command), "version option should be valid");
    expect(command.show_version, "version option should request version output");
}

void parses_build_path() {
    const auto command = parse({"codesplit", "analyze", "large.cpp", "--build-path", "out/debug"});

    expect(static_cast<bool>(command), "command with build path should be valid");
    expect(command.build_path == "out/debug", "custom build path should be preserved");
}

void parses_maximum_size() {
    const auto command = parse({"codesplit", "analyze", "large.cpp", "--max-size-kb", "256"});

    expect(static_cast<bool>(command), "numeric maximum size should be valid");
    expect(command.max_size_kib == 256, "maximum size should be preserved");
}

void rejects_invalid_maximum_sizes() {
    const auto zero = parse({"codesplit", "analyze", "large.cpp", "--max-size-kb", "0"});
    const auto text = parse({"codesplit", "analyze", "large.cpp", "--max-size-kb", "large"});
    const auto missing = parse({"codesplit", "analyze", "large.cpp", "--max-size-kb"});

    const auto overflowing_value = std::numeric_limits<std::uintmax_t>::max() / 1024U + 1U;
    const auto overflow = parse(
        {"codesplit", "analyze", "large.cpp", "--max-size-kb", std::to_string(overflowing_value)});

    expect(!zero, "zero maximum size should be rejected");
    expect(!text, "non-numeric maximum size should be rejected");
    expect(!missing, "missing maximum size should be rejected");
    expect(!overflow, "maximum size that overflows bytes should be rejected");
}

void parses_report_format() {
    const auto command = parse({"codesplit", "analyze", "large.cpp", "--format", "json"});

    expect(static_cast<bool>(command), "JSON report format should be valid");
    expect(command.report_format == codesplit::cli::ReportFormat::json,
           "report format should be JSON");
}

void rejects_invalid_report_formats() {
    const auto unknown = parse({"codesplit", "analyze", "large.cpp", "--format", "xml"});
    const auto missing = parse({"codesplit", "analyze", "large.cpp", "--format"});

    expect(!unknown, "unknown report format should be rejected");
    expect(!missing, "missing report format should be rejected");
}

void rejects_unknown_command() {
    const auto command = parse({"codesplit", "split", "large.cpp"});

    expect(!command, "unknown command should be rejected");
    expect(command.error == "Unknown command: split", "error should identify the command");
}

void rejects_missing_build_path_value() {
    const auto command = parse({"codesplit", "analyze", "large.cpp", "--build-path"});

    expect(!command, "missing build path value should be rejected");
    expect(command.error == "Missing value for --build-path.",
           "error should identify the missing value");
}

void exposes_stable_process_exit_codes() {
    using codesplit::cli::ExitCode;
    using codesplit::cli::process_exit_code;

    expect(process_exit_code(ExitCode::success) == 0, "success exit code should remain stable");
    expect(process_exit_code(ExitCode::runtime_error) == 1,
           "runtime error exit code should remain stable");
    expect(process_exit_code(ExitCode::invalid_command) == 2,
           "invalid command exit code should remain stable");
    expect(process_exit_code(ExitCode::transformation_blocked) == 3,
           "blocked transformation exit code should remain stable");
    expect(process_exit_code(ExitCode::apply_failed) == 4,
           "apply failure exit code should remain stable");
}

} // namespace

int main() {
    parses_analyze_command();
    parses_plan_move_command();
    rejects_incomplete_plan_move_command();
    parses_dry_run_move_command();
    parses_confirmed_apply_move_command();
    parses_version_option();
    parses_build_path();
    parses_maximum_size();
    rejects_invalid_maximum_sizes();
    parses_report_format();
    rejects_invalid_report_formats();
    rejects_unknown_command();
    rejects_missing_build_path_value();
    exposes_stable_process_exit_codes();

    if (failure_count == 0) {
        std::cout << "All command-line tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

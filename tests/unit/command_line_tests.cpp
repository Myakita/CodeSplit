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

} // namespace

int main() {
    parses_analyze_command();
    parses_build_path();
    parses_maximum_size();
    rejects_invalid_maximum_sizes();
    rejects_unknown_command();
    rejects_missing_build_path_value();

    if (failure_count == 0) {
        std::cout << "All command-line tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

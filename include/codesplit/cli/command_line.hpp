#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace codesplit::cli {

enum class Operation {
    analyze,
};

struct CommandLine {
    Operation operation{Operation::analyze};
    std::filesystem::path input_path;
    std::filesystem::path build_path{"build"};
    std::uintmax_t max_size_kib{100};
    bool show_help{false};
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] CommandLine parse_command_line(int argc, char* argv[]);
[[nodiscard]] std::string usage();

} // namespace codesplit::cli

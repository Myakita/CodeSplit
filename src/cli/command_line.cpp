#include "codesplit/cli/command_line.hpp"

#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>

namespace codesplit::cli {
namespace {

bool is_help_option(std::string_view argument) { return argument == "--help" || argument == "-h"; }

bool parse_max_size(std::string_view argument, std::uintmax_t& value) {
    const auto conversion =
        std::from_chars(argument.data(), argument.data() + argument.size(), value);
    const auto maximum_kib = std::numeric_limits<std::uintmax_t>::max() / 1024U;

    return conversion.ec == std::errc{} && conversion.ptr == argument.data() + argument.size() &&
           value > 0 && value <= maximum_kib;
}

bool parse_report_format(std::string_view argument, ReportFormat& format) {
    if (argument == "text") {
        format = ReportFormat::text;
        return true;
    }

    if (argument == "json") {
        format = ReportFormat::json;
        return true;
    }

    return false;
}

} // namespace

CommandLine parse_command_line(int argc, char* argv[]) {
    CommandLine result;

    if (argc == 2 && is_help_option(argv[1])) {
        result.show_help = true;
        return result;
    }

    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        result.show_version = true;
        return result;
    }

    if (argc < 3) {
        result.error = "Missing command or input file.";
        return result;
    }

    const std::string_view operation{argv[1]};
    if (operation == "analyze") {
        result.operation = Operation::analyze;
    } else if (operation == "plan-move") {
        result.operation = Operation::plan_move;
    } else {
        result.error = "Unknown command: " + std::string{argv[1]};
        return result;
    }

    result.input_path = argv[2];

    for (int index = 3; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (is_help_option(argument)) {
            result.show_help = true;
            continue;
        }

        if (argument == "--build-path") {
            if (++index >= argc) {
                result.error = "Missing value for --build-path.";
                return result;
            }

            result.build_path = argv[index];
            continue;
        }

        if (argument == "--symbol-id") {
            if (++index >= argc) {
                result.error = "Missing value for --symbol-id.";
                return result;
            }

            result.symbol_id = argv[index];
            continue;
        }

        if (argument == "--target") {
            if (++index >= argc) {
                result.error = "Missing value for --target.";
                return result;
            }

            result.target_path = argv[index];
            continue;
        }

        if (argument == "--max-size-kb") {
            if (++index >= argc) {
                result.error = "Missing value for --max-size-kb.";
                return result;
            }

            const std::string_view value{argv[index]};
            if (!parse_max_size(value, result.max_size_kib)) {
                result.error = "Invalid value for --max-size-kb: " + std::string{value};
                return result;
            }

            continue;
        }

        if (argument == "--format") {
            if (++index >= argc) {
                result.error = "Missing value for --format.";
                return result;
            }

            const std::string_view value{argv[index]};
            if (!parse_report_format(value, result.report_format)) {
                result.error = "Invalid value for --format: " + std::string{value};
                return result;
            }

            continue;
        }

        result.error = "Unknown option: " + std::string{argument};
        return result;
    }

    if (result.show_help) {
        return result;
    }

    if (result.operation == Operation::analyze &&
        (!result.symbol_id.empty() || !result.target_path.empty())) {
        result.error = "Options --symbol-id and --target are only valid for plan-move.";
        return result;
    }

    if (result.operation == Operation::plan_move) {
        if (result.symbol_id.empty()) {
            result.error = "Missing required --symbol-id for plan-move.";
            return result;
        }
        if (result.target_path.empty()) {
            result.error = "Missing required --target for plan-move.";
            return result;
        }
    }

    return result;
}

std::string usage() {
    return "Usage:\n"
           "  codesplit analyze <file> [--build-path <directory>] [--max-size-kb <number>] "
           "[--format <text|json>]\n"
           "  codesplit plan-move <file> --symbol-id <usr> --target <file> "
           "[--build-path <directory>] [--max-size-kb <number>] [--format <text|json>]\n"
           "  codesplit --version\n"
           "  codesplit --help\n";
}

} // namespace codesplit::cli

#include "codesplit/cli/command_line.hpp"

#include <string_view>

namespace codesplit::cli {
namespace {

bool is_help_option(std::string_view argument) {
    return argument == "--help" || argument == "-h";
}

} // namespace

CommandLine parse_command_line(int argc, char* argv[]) {
    CommandLine result;

    if (argc == 2 && is_help_option(argv[1])) {
        result.show_help = true;
        return result;
    }

    if (argc < 3) {
        result.error = "Missing command or input file.";
        return result;
    }

    if (std::string_view{argv[1]} != "analyze") {
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

        result.error = "Unknown option: " + std::string{argument};
        return result;
    }

    return result;
}

std::string usage() {
    return "Usage:\n"
           "  codesplit analyze <file> [--build-path <directory>]\n"
           "  codesplit --help\n";
}

} // namespace codesplit::cli

#include "codesplit/cli/command_line.hpp"

#include <iostream>

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

    std::cout << "Analysis is not implemented yet: " << command.input_path.string() << '\n';
    return 0;
}

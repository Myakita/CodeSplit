#include "codesplit/analysis/source_file.hpp"
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

    constexpr std::uintmax_t bytes_per_kib = 1024U;
    const auto size_limit_bytes = command.max_size_kib * bytes_per_kib;
    const auto analysis =
        codesplit::analysis::analyze_source_file(command.input_path, size_limit_bytes);
    if (!analysis) {
        std::cerr << analysis.error << '\n';
        return 1;
    }

    std::cout << "File: " << analysis.info.path.string() << '\n';
    std::cout << "Size: " << analysis.info.size_bytes << " bytes\n";
    std::cout << "Lines: " << analysis.info.line_count << '\n';
    std::cout << "Exceeds " << command.max_size_kib
              << " KiB: " << (analysis.info.exceeds_size_limit ? "yes" : "no") << '\n';
    return 0;
}

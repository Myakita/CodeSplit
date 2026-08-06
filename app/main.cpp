#include "codesplit/analysis/compilation_database.hpp"
#include "codesplit/analysis/source_file.hpp"
#include "codesplit/cli/command_line.hpp"
#include "codesplit/reporting/json_report.hpp"
#include "codesplit/reporting/text_report.hpp"

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

    const auto compilation =
        codesplit::analysis::load_compilation_command(command.build_path, command.input_path);

    if (command.report_format == codesplit::cli::ReportFormat::json) {
        std::cout << codesplit::reporting::format_json_report(analysis.info, command.max_size_kib,
                                                              compilation);
    } else {
        std::cout << codesplit::reporting::format_text_report(analysis.info, command.max_size_kib,
                                                              compilation);
    }

    return 0;
}

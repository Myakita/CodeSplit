#include "codesplit/analysis/callable_inventory.hpp"
#include "codesplit/analysis/source_file.hpp"
#include "codesplit/cli/command_line.hpp"
#include "codesplit/planning/move_apply.hpp"
#include "codesplit/planning/move_dry_run.hpp"
#include "codesplit/planning/move_plan.hpp"
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

    const auto inventory = codesplit::analysis::inventory_callables(
        command.build_path, command.input_path, size_limit_bytes);

    if (command.operation == codesplit::cli::Operation::plan_move ||
        command.operation == codesplit::cli::Operation::dry_run_move ||
        command.operation == codesplit::cli::Operation::apply_move) {
        const auto plan = codesplit::planning::plan_callable_move(
            command.input_path, command.target_path, command.symbol_id, inventory);
        if (command.operation == codesplit::cli::Operation::dry_run_move ||
            command.operation == codesplit::cli::Operation::apply_move) {
            const auto dry_run = codesplit::planning::draft_callable_move(plan);
            if (command.operation == codesplit::cli::Operation::apply_move) {
                const auto result = codesplit::planning::apply_callable_move(dry_run);
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

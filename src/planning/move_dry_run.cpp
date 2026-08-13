#include "codesplit/planning/move_dry_run.hpp"

#include <fstream>
#include <iterator>
#include <string_view>

namespace codesplit::planning {
namespace {

std::filesystem::path normalized_absolute_path(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    return (error ? path : absolute).lexically_normal();
}

void add_blocker(MoveDryRun& dry_run, MoveDryRunBlockerKind kind, std::string detail) {
    dry_run.blockers.push_back({.kind = kind, .detail = std::move(detail)});
}

std::string_view line_ending_for(const std::string& source_text, std::size_t end_offset) {
    if (source_text.compare(end_offset, 2, "\r\n") == 0 ||
        source_text.find("\r\n") != std::string::npos) {
        return "\r\n";
    }
    return "\n";
}

} // namespace

bool replacements_overlap(const TextReplacement& left, const TextReplacement& right) {
    if (normalized_absolute_path(left.path) != normalized_absolute_path(right.path)) {
        return false;
    }
    if (left.begin_offset == left.end_offset && right.begin_offset == right.end_offset) {
        return left.begin_offset == right.begin_offset;
    }
    return left.begin_offset < right.end_offset && right.begin_offset < left.end_offset;
}

MoveDryRun draft_callable_move(const MovePlan& plan) {
    MoveDryRun dry_run{.plan = plan};
    if (!plan) {
        add_blocker(dry_run, MoveDryRunBlockerKind::plan_blocked, "move plan contains blockers");
        return dry_run;
    }
    if (!plan.definition.has_value()) {
        add_blocker(dry_run, MoveDryRunBlockerKind::invalid_source_range,
                    "move plan has no definition range");
        return dry_run;
    }

    std::error_code target_error;
    if (std::filesystem::exists(plan.target_path, target_error)) {
        add_blocker(dry_run, MoveDryRunBlockerKind::target_exists, plan.target_path.string());
        return dry_run;
    }

    std::ifstream source{plan.source_path, std::ios::binary};
    if (!source) {
        add_blocker(dry_run, MoveDryRunBlockerKind::source_read_failed, plan.source_path.string());
        return dry_run;
    }
    const std::string source_text{std::istreambuf_iterator<char>{source},
                                  std::istreambuf_iterator<char>{}};
    const auto begin = plan.definition->begin_offset;
    const auto end = plan.definition->end_offset;
    if (begin >= end || end > source_text.size()) {
        add_blocker(dry_run, MoveDryRunBlockerKind::invalid_source_range,
                    std::to_string(begin) + "-" + std::to_string(end));
        return dry_run;
    }

    const auto definition =
        source_text.substr(static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin));
    const auto line_ending = line_ending_for(source_text, static_cast<std::size_t>(end));
    dry_run.replacements = {
        {.path = plan.source_path,
         .begin_offset = begin,
         .end_offset = end,
         .expected_text = definition},
        {.path = plan.target_path,
         .begin_offset = 0,
         .end_offset = 0,
         .replacement_text = definition + std::string{line_ending}},
    };
    if (replacements_overlap(dry_run.replacements[0], dry_run.replacements[1])) {
        dry_run.replacements.clear();
        add_blocker(dry_run, MoveDryRunBlockerKind::overlapping_replacements,
                    "source removal overlaps target insertion");
    }
    return dry_run;
}

} // namespace codesplit::planning

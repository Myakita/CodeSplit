#pragma once

#include "codesplit/planning/move_plan.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace codesplit::planning {

enum class MoveDryRunBlockerKind {
    plan_blocked,
    source_read_failed,
    target_exists,
    invalid_source_range,
    overlapping_replacements,
};

struct TextReplacement {
    std::filesystem::path path;
    std::uintmax_t begin_offset = 0;
    std::uintmax_t end_offset = 0;
    std::string expected_text;
    std::string replacement_text;
};

struct MoveDryRunBlocker {
    MoveDryRunBlockerKind kind = MoveDryRunBlockerKind::plan_blocked;
    std::string detail;
};

struct MoveDryRun {
    MovePlan plan;
    std::vector<TextReplacement> replacements;
    std::vector<MoveDryRunBlocker> blockers;
    bool read_only = true;

    [[nodiscard]] explicit operator bool() const noexcept { return blockers.empty(); }
};

[[nodiscard]] bool replacements_overlap(const TextReplacement& left, const TextReplacement& right);
[[nodiscard]] MoveDryRun draft_callable_move(const MovePlan& plan);

} // namespace codesplit::planning

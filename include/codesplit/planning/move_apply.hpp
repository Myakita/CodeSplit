#pragma once

#include "codesplit/planning/move_dry_run.hpp"

#include <functional>
#include <string>
#include <vector>

namespace codesplit::planning {

enum class MoveApplyBlockerKind {
    dry_run_blocked,
    invalid_replacement_set,
    source_changed,
    staging_failed,
    commit_failed,
    validation_failed,
    rollback_failed,
};

struct MoveApplyBlocker {
    MoveApplyBlockerKind kind = MoveApplyBlockerKind::dry_run_blocked;
    std::string detail;
};

struct MoveApplyResult {
    MoveDryRun dry_run;
    std::vector<MoveApplyBlocker> blockers;
    std::vector<std::string> warnings;
    bool applied = false;
    bool validated = false;
    bool rolled_back = false;

    [[nodiscard]] explicit operator bool() const noexcept { return applied && blockers.empty(); }
};

struct MoveValidationResult {
    bool success = false;
    std::string detail;
};

using MoveValidator = std::function<MoveValidationResult(const std::filesystem::path& source_path,
                                                         const std::filesystem::path& target_path)>;

[[nodiscard]] MoveApplyResult apply_callable_move(const MoveDryRun& dry_run,
                                                  const MoveValidator& validator = {});

} // namespace codesplit::planning

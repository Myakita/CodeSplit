#pragma once

#include "codesplit/planning/move_dry_run.hpp"

#include <string>
#include <vector>

namespace codesplit::planning {

enum class MoveApplyBlockerKind {
    dry_run_blocked,
    invalid_replacement_set,
    source_changed,
    staging_failed,
    commit_failed,
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
    bool rolled_back = false;

    [[nodiscard]] explicit operator bool() const noexcept { return applied && blockers.empty(); }
};

[[nodiscard]] MoveApplyResult apply_callable_move(const MoveDryRun& dry_run);

} // namespace codesplit::planning

#pragma once

#include "codesplit/analysis/callable_inventory.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codesplit::planning {

enum class MovePlanBlockerKind {
    inventory_unavailable,
    symbol_not_found,
    source_target_collision,
    not_free_function,
    callable_constraint,
    non_external_linkage,
    outgoing_dependency,
    incoming_dependency_without_declaration,
    macro_dependency,
};

enum class MovePlanStepKind {
    copy_definition,
    remove_definition,
    validate_frontend,
    build_and_test,
};

struct MovePlanBlocker {
    MovePlanBlockerKind kind = MovePlanBlockerKind::inventory_unavailable;
    std::string detail;
};

struct MovePlanStep {
    MovePlanStepKind kind = MovePlanStepKind::copy_definition;
};

struct MovePlan {
    std::filesystem::path source_path;
    std::filesystem::path target_path;
    std::string symbol_id;
    std::string qualified_name;
    std::optional<analysis::SourceRange> definition;
    std::vector<MovePlanBlocker> blockers;
    std::vector<MovePlanStep> steps;
    bool read_only = true;

    [[nodiscard]] explicit operator bool() const noexcept { return blockers.empty(); }
};

[[nodiscard]] MovePlan plan_callable_move(const std::filesystem::path& source_path,
                                          const std::filesystem::path& target_path,
                                          const std::string& symbol_id,
                                          const analysis::CallableInventoryResult& inventory);

} // namespace codesplit::planning

#include "codesplit/planning/move_plan.hpp"

#include <algorithm>
#include <string_view>

namespace codesplit::planning {
namespace {

std::filesystem::path normalized_absolute_path(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    return (error ? path : absolute).lexically_normal();
}

std::string_view constraint_name(analysis::CallableConstraint constraint) {
    switch (constraint) {
    case analysis::CallableConstraint::macro_expansion:
        return "macro_expansion";
    case analysis::CallableConstraint::source_range_unavailable:
        return "source_range_unavailable";
    case analysis::CallableConstraint::exceeds_size_limit:
        return "exceeds_size_limit";
    case analysis::CallableConstraint::delegation_unsupported:
        return "delegation_unsupported";
    }
    return "unknown";
}

std::string_view dependency_name(analysis::CallableDependencyKind kind) {
    switch (kind) {
    case analysis::CallableDependencyKind::direct_call:
        return "direct_call";
    case analysis::CallableDependencyKind::type_reference:
        return "type_reference";
    case analysis::CallableDependencyKind::global_read:
        return "global_read";
    case analysis::CallableDependencyKind::global_write:
        return "global_write";
    }
    return "unknown";
}

void add_blocker(MovePlan& plan, MovePlanBlockerKind kind, std::string detail) {
    plan.blockers.push_back({.kind = kind, .detail = std::move(detail)});
}

bool same_normalized_path(const std::filesystem::path& left, const std::filesystem::path& right) {
    return normalized_absolute_path(left) == normalized_absolute_path(right);
}

} // namespace

MovePlan plan_callable_move(const std::filesystem::path& source_path,
                            const std::filesystem::path& target_path, const std::string& symbol_id,
                            const analysis::CallableInventoryResult& inventory) {
    MovePlan plan{
        .source_path = source_path,
        .target_path = target_path,
        .symbol_id = symbol_id,
    };

    if (normalized_absolute_path(source_path) == normalized_absolute_path(target_path)) {
        add_blocker(plan, MovePlanBlockerKind::source_target_collision,
                    "source and target paths resolve to the same lexical path");
    }

    if (!inventory) {
        add_blocker(plan, MovePlanBlockerKind::inventory_unavailable, inventory.error);
        return plan;
    }

    const auto callable =
        std::ranges::find(inventory.callables, symbol_id, &analysis::CallableDefinition::symbol_id);
    if (callable == inventory.callables.end()) {
        add_blocker(plan, MovePlanBlockerKind::symbol_not_found, symbol_id);
        return plan;
    }

    plan.qualified_name = callable->qualified_name;
    plan.callable_name = callable->name;
    plan.implementation_name = callable->name + "_codesplit_implementation";
    plan.parameter_names = callable->parameter_names;
    plan.returns_void = callable->returns_void;
    plan.name_offset = callable->name_offset;
    plan.body = callable->body;
    plan.enclosing_namespaces = callable->enclosing_namespaces;
    plan.definition = analysis::SourceRange{
        .path = source_path,
        .begin_offset = callable->begin_offset,
        .end_offset = callable->end_offset,
        .begin_line = callable->begin_line,
        .end_line = callable->end_line,
    };

    if (callable->kind != analysis::CallableKind::free_function) {
        add_blocker(plan, MovePlanBlockerKind::not_free_function,
                    "only free functions are supported by this planner slice");
    }
    if (callable->linkage != analysis::SymbolLinkage::external) {
        add_blocker(plan, MovePlanBlockerKind::non_external_linkage,
                    "callable must have external linkage");
    }
    for (const auto constraint : callable->constraints) {
        add_blocker(plan, MovePlanBlockerKind::callable_constraint,
                    std::string{constraint_name(constraint)});
    }

    for (const auto& dependency : inventory.dependencies) {
        if (dependency.source_symbol_id == symbol_id && dependency.target_symbol_id != symbol_id) {
            add_blocker(plan, MovePlanBlockerKind::outgoing_dependency,
                        std::string{dependency_name(dependency.kind)} + ": " +
                            dependency.target_qualified_name);
        }
        if (dependency.target_symbol_id == symbol_id && dependency.source_symbol_id != symbol_id &&
            !callable->declaration.has_value()) {
            add_blocker(plan, MovePlanBlockerKind::incoming_dependency_without_declaration,
                        dependency.source_qualified_name);
        }
    }

    for (const auto& macro : inventory.macros) {
        if (macro.source_symbol_id == symbol_id) {
            add_blocker(plan, MovePlanBlockerKind::macro_dependency, macro.macro_name);
        }
    }

    if (callable->declaration.has_value() &&
        !same_normalized_path(callable->declaration->path, source_path)) {
        const auto include = std::ranges::find_if(inventory.includes, [&](const auto& candidate) {
            return same_normalized_path(candidate.resolved_path, callable->declaration->path);
        });
        if (include == inventory.includes.end()) {
            add_blocker(plan, MovePlanBlockerKind::declaration_include_unavailable,
                        callable->declaration->path.string());
        } else {
            plan.declaration_include = *include;
        }
    }

    if (plan.blockers.empty()) {
        plan.steps = {
            {.kind = MovePlanStepKind::copy_definition},
            {.kind = MovePlanStepKind::remove_definition},
            {.kind = MovePlanStepKind::validate_frontend},
            {.kind = MovePlanStepKind::build_and_test},
        };
    }

    return plan;
}

} // namespace codesplit::planning

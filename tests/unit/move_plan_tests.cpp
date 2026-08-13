#include "codesplit/planning/move_plan.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

int failure_count = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

bool has_blocker(const codesplit::planning::MovePlan& plan,
                 codesplit::planning::MovePlanBlockerKind kind) {
    return std::ranges::find(plan.blockers, kind, &codesplit::planning::MovePlanBlocker::kind) !=
           plan.blockers.end();
}

codesplit::analysis::CallableDefinition movable_callable() {
    return {
        .kind = codesplit::analysis::CallableKind::free_function,
        .linkage = codesplit::analysis::SymbolLinkage::external,
        .qualified_name = "sample::isolated",
        .symbol_id = "c:@N@sample@F@isolated#I#",
        .enclosing_namespaces = {"sample"},
        .declaration =
            codesplit::analysis::SourceRange{
                .path = "include/isolated.hpp",
                .begin_offset = 20,
                .end_offset = 38,
                .begin_line = 2,
                .end_line = 2,
            },
        .begin_offset = 100,
        .end_offset = 160,
        .size_bytes = 60,
        .begin_line = 8,
        .end_line = 10,
    };
}

void plans_isolated_external_function() {
    codesplit::analysis::CallableInventoryResult inventory;
    inventory.compilation.command.working_directory = "build";
    inventory.callables.push_back(movable_callable());
    inventory.includes.push_back({
        .kind = codesplit::analysis::IncludeKind::quoted,
        .written_name = "isolated.hpp",
        .resolved_path = "include/isolated.hpp",
    });

    const auto plan = codesplit::planning::plan_callable_move(
        "src/large.cpp", "src/isolated.cpp", inventory.callables.front().symbol_id, inventory);

    expect(static_cast<bool>(plan), "isolated external function should produce a ready plan");
    expect(plan.read_only, "first move plan should be read-only");
    expect(plan.qualified_name == "sample::isolated", "plan should retain the callable name");
    expect(plan.declaration_include.has_value(), "plan should retain the declaring include");
    expect(plan.enclosing_namespaces == std::vector<std::string>{"sample"},
           "plan should retain lexical namespace context");
    expect(plan.definition.has_value(), "plan should retain the definition range");
    if (plan.definition.has_value()) {
        expect(plan.definition->path == "src/large.cpp",
               "definition range should identify the source file");
        expect(plan.definition->begin_offset == 100 && plan.definition->end_offset == 160,
               "definition range should retain exact byte offsets");
    }
    expect(plan.steps.size() == 4, "ready plan should contain four ordered steps");
    if (plan.steps.size() == 4) {
        expect(plan.steps[0].kind == codesplit::planning::MovePlanStepKind::create_implementation,
               "plan should first create the extracted implementation");
        expect(plan.steps[1].kind ==
                   codesplit::planning::MovePlanStepKind::replace_body_with_delegate,
               "plan should retain a delegating original function");
        expect(plan.steps[2].kind == codesplit::planning::MovePlanStepKind::validate_frontend,
               "plan should repeat frontend analysis");
        expect(plan.steps[3].kind == codesplit::planning::MovePlanStepKind::build_and_test,
               "plan should finish with build and tests");
    }
}

void blocks_unsupported_or_unsafe_function() {
    codesplit::analysis::CallableInventoryResult inventory;
    inventory.compilation.command.working_directory = "build";
    auto callable = movable_callable();
    callable.kind = codesplit::analysis::CallableKind::method;
    callable.linkage = codesplit::analysis::SymbolLinkage::internal;
    callable.constraints.push_back(codesplit::analysis::CallableConstraint::macro_expansion);
    callable.declaration.reset();
    inventory.callables.push_back(callable);
    inventory.dependencies.push_back({
        .kind = codesplit::analysis::CallableDependencyKind::direct_call,
        .source_symbol_id = callable.symbol_id,
        .source_qualified_name = callable.qualified_name,
        .target_symbol_id = "c:@F@helper#",
        .target_qualified_name = "helper",
    });
    inventory.dependencies.push_back({
        .kind = codesplit::analysis::CallableDependencyKind::direct_call,
        .source_symbol_id = "c:@F@caller#",
        .source_qualified_name = "caller",
        .target_symbol_id = callable.symbol_id,
        .target_qualified_name = callable.qualified_name,
    });
    inventory.macros.push_back({
        .source_symbol_id = callable.symbol_id,
        .source_qualified_name = callable.qualified_name,
        .macro_name = "GENERATED",
    });

    const auto plan = codesplit::planning::plan_callable_move("src/large.cpp", "src/method.cpp",
                                                              callable.symbol_id, inventory);

    expect(!plan, "unsupported callable should produce a blocked plan");
    expect(plan.steps.empty(), "blocked plan should not propose transformation steps");
    expect(has_blocker(plan, codesplit::planning::MovePlanBlockerKind::not_free_function),
           "method should be blocked");
    expect(has_blocker(plan, codesplit::planning::MovePlanBlockerKind::non_external_linkage),
           "internal linkage should be blocked");
    expect(has_blocker(plan, codesplit::planning::MovePlanBlockerKind::callable_constraint),
           "callable constraint should be blocked");
    expect(has_blocker(plan, codesplit::planning::MovePlanBlockerKind::outgoing_dependency),
           "outgoing dependency should be blocked");
    expect(has_blocker(
               plan,
               codesplit::planning::MovePlanBlockerKind::incoming_dependency_without_declaration),
           "incoming dependency without a declaration should be blocked");
    expect(has_blocker(plan, codesplit::planning::MovePlanBlockerKind::macro_dependency),
           "macro dependency should be blocked");
}

void blocks_invalid_request() {
    codesplit::analysis::CallableInventoryResult unavailable;
    unavailable.error = "frontend failed";
    const auto unavailable_plan = codesplit::planning::plan_callable_move(
        "src/large.cpp", "src/new.cpp", "missing", unavailable);
    expect(has_blocker(unavailable_plan,
                       codesplit::planning::MovePlanBlockerKind::inventory_unavailable),
           "unavailable inventory should be blocked");

    codesplit::analysis::CallableInventoryResult inventory;
    inventory.compilation.command.working_directory = "build";
    const auto missing = codesplit::planning::plan_callable_move("src/large.cpp", "src/new.cpp",
                                                                 "missing", inventory);
    expect(has_blocker(missing, codesplit::planning::MovePlanBlockerKind::symbol_not_found),
           "unknown symbol should be blocked");

    inventory.callables.push_back(movable_callable());
    const auto collision = codesplit::planning::plan_callable_move(
        "src/large.cpp", "src/large.cpp", inventory.callables.front().symbol_id, inventory);
    expect(
        has_blocker(collision, codesplit::planning::MovePlanBlockerKind::source_target_collision),
        "source and target collision should be blocked");
}

void blocks_declaration_without_direct_include() {
    codesplit::analysis::CallableInventoryResult inventory;
    inventory.compilation.command.working_directory = "build";
    inventory.callables.push_back(movable_callable());

    const auto plan = codesplit::planning::plan_callable_move(
        "src/large.cpp", "src/isolated.cpp", inventory.callables.front().symbol_id, inventory);

    expect(has_blocker(plan,
                       codesplit::planning::MovePlanBlockerKind::declaration_include_unavailable),
           "header declaration without a direct include should block target generation");
}

} // namespace

int main() {
    plans_isolated_external_function();
    blocks_unsupported_or_unsafe_function();
    blocks_invalid_request();
    blocks_declaration_without_direct_include();

    if (failure_count == 0) {
        std::cout << "All move-plan tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

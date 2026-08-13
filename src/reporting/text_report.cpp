#include "codesplit/reporting/text_report.hpp"

#include <sstream>
#include <string_view>

namespace codesplit::reporting {
namespace {

std::string_view callable_kind_name(analysis::CallableKind kind) {
    switch (kind) {
    case analysis::CallableKind::free_function:
        return "free function";
    case analysis::CallableKind::method:
        return "method";
    }
    return "unknown";
}

std::string_view linkage_name(analysis::SymbolLinkage linkage) {
    switch (linkage) {
    case analysis::SymbolLinkage::none:
        return "none";
    case analysis::SymbolLinkage::internal:
        return "internal";
    case analysis::SymbolLinkage::unique_external:
        return "unique external";
    case analysis::SymbolLinkage::module:
        return "module";
    case analysis::SymbolLinkage::external:
        return "external";
    }
    return "unknown";
}

std::string_view constraint_name(analysis::CallableConstraint constraint) {
    switch (constraint) {
    case analysis::CallableConstraint::macro_expansion:
        return "macro_expansion";
    case analysis::CallableConstraint::source_range_unavailable:
        return "source_range_unavailable";
    case analysis::CallableConstraint::exceeds_size_limit:
        return "exceeds_size_limit";
    }
    return "unknown";
}

std::string_view diagnostic_severity_name(analysis::FrontendDiagnosticSeverity severity) {
    switch (severity) {
    case analysis::FrontendDiagnosticSeverity::note:
        return "note";
    case analysis::FrontendDiagnosticSeverity::remark:
        return "remark";
    case analysis::FrontendDiagnosticSeverity::warning:
        return "warning";
    case analysis::FrontendDiagnosticSeverity::error:
        return "error";
    case analysis::FrontendDiagnosticSeverity::fatal:
        return "fatal";
    }
    return "unknown";
}

std::string_view dependency_kind_name(analysis::CallableDependencyKind kind) {
    switch (kind) {
    case analysis::CallableDependencyKind::direct_call:
        return "direct call";
    case analysis::CallableDependencyKind::type_reference:
        return "type reference";
    case analysis::CallableDependencyKind::global_read:
        return "global read";
    case analysis::CallableDependencyKind::global_write:
        return "global write";
    }
    return "unknown";
}

std::string_view include_kind_name(analysis::IncludeKind kind) {
    switch (kind) {
    case analysis::IncludeKind::quoted:
        return "quoted";
    case analysis::IncludeKind::angled:
        return "angled";
    }
    return "unknown";
}

std::string_view blocker_kind_name(planning::MovePlanBlockerKind kind) {
    using enum planning::MovePlanBlockerKind;
    switch (kind) {
    case inventory_unavailable:
        return "inventory unavailable";
    case symbol_not_found:
        return "symbol not found";
    case source_target_collision:
        return "source and target collide";
    case not_free_function:
        return "not a free function";
    case callable_constraint:
        return "callable constraint";
    case non_external_linkage:
        return "non-external linkage";
    case outgoing_dependency:
        return "outgoing dependency";
    case incoming_dependency_without_declaration:
        return "incoming dependency without declaration";
    case macro_dependency:
        return "macro dependency";
    }
    return "unknown";
}

std::string_view step_name(planning::MovePlanStepKind kind) {
    using enum planning::MovePlanStepKind;
    switch (kind) {
    case copy_definition:
        return "copy definition to target";
    case remove_definition:
        return "remove definition from source";
    case validate_frontend:
        return "repeat frontend analysis";
    case build_and_test:
        return "build and test affected project";
    }
    return "unknown";
}

void append_diagnostic(std::ostringstream& report, const analysis::FrontendDiagnostic& diagnostic) {
    report << "- " << diagnostic_severity_name(diagnostic.severity);
    if (!diagnostic.path.empty()) {
        report << ' ' << diagnostic.path.string() << ':' << diagnostic.line << ':'
               << diagnostic.column;
    }
    report << ": " << diagnostic.message << '\n';
}

void append_callable(std::ostringstream& report, const analysis::CallableDefinition& callable) {
    report << "- " << callable_kind_name(callable.kind) << ' ' << callable.qualified_name
           << ": lines " << callable.begin_line << '-' << callable.end_line << ", "
           << callable.size_bytes << " bytes";
    if (!callable.constraints.empty()) {
        report << " [";
        for (std::size_t index = 0; index < callable.constraints.size(); ++index) {
            if (index != 0) {
                report << ", ";
            }
            report << constraint_name(callable.constraints[index]);
        }
        report << ']';
    }
    report << '\n';
    report << "  Linkage: " << linkage_name(callable.linkage) << '\n';
    if (callable.in_anonymous_namespace) {
        report << "  Anonymous namespace: yes\n";
    }
    if (!callable.symbol_id.empty()) {
        report << "  Symbol ID: " << callable.symbol_id << '\n';
    }
    if (callable.declaration.has_value()) {
        report << "  Declaration: " << callable.declaration->path.string() << ", lines "
               << callable.declaration->begin_line << '-' << callable.declaration->end_line << '\n';
    }
    if (callable.owning_record.has_value()) {
        report << "  Owning record: " << callable.owning_record->path.string() << ", lines "
               << callable.owning_record->begin_line << '-' << callable.owning_record->end_line
               << '\n';
    }
}

} // namespace

std::string format_text_report(const analysis::SourceFileInfo& info, std::uintmax_t size_limit_kib,
                               const analysis::CallableInventoryResult& inventory) {
    std::ostringstream report;
    report << "File: " << info.path.string() << '\n';
    report << "Size: " << info.size_bytes << " bytes\n";
    report << "Lines: " << info.line_count << '\n';
    report << "Exceeds " << size_limit_kib << " KiB: " << (info.exceeds_size_limit ? "yes" : "no")
           << '\n';
    report << "Compilation command: " << (inventory.compilation ? "available" : "unavailable")
           << '\n';
    if (!inventory.compilation) {
        report << "Reason: " << inventory.compilation.error << '\n';
    }
    report << "Callable inventory: " << (inventory ? "available" : "unavailable") << '\n';
    if (!inventory && inventory.error != inventory.compilation.error) {
        report << "Reason: " << inventory.error << '\n';
    }
    if (!inventory.diagnostics.empty()) {
        report << "Frontend diagnostics: " << inventory.diagnostics.size() << '\n';
        for (const auto& diagnostic : inventory.diagnostics) {
            append_diagnostic(report, diagnostic);
        }
    }
    if (inventory) {
        report << "Callable definitions: " << inventory.callables.size() << '\n';
        for (const auto& callable : inventory.callables) {
            append_callable(report, callable);
        }
        if (!inventory.dependencies.empty()) {
            report << "Callable dependencies: " << inventory.dependencies.size() << '\n';
            for (const auto& dependency : inventory.dependencies) {
                report << "- " << dependency_kind_name(dependency.kind) << ' '
                       << dependency.source_qualified_name << " -> "
                       << dependency.target_qualified_name << '\n';
            }
        }
        if (!inventory.includes.empty()) {
            report << "Include dependencies: " << inventory.includes.size() << '\n';
            for (const auto& include : inventory.includes) {
                report << "- " << include_kind_name(include.kind) << ' ' << include.written_name
                       << " -> " << include.resolved_path.string() << " at "
                       << include.origin.path.string() << ':' << include.origin.begin_line << '\n';
            }
        }
        if (!inventory.macros.empty()) {
            report << "Macro dependencies: " << inventory.macros.size() << '\n';
            for (const auto& macro : inventory.macros) {
                report << "- " << macro.source_qualified_name << " -> " << macro.macro_name << '\n';
                if (macro.definition.has_value()) {
                    report << "  Definition: " << macro.definition->path.string() << ", lines "
                           << macro.definition->begin_line << '-' << macro.definition->end_line
                           << '\n';
                }
                for (const auto& expansion : macro.expansions) {
                    report << "  Expansion: " << expansion.path.string() << ':'
                           << expansion.begin_line << '\n';
                }
            }
        }
    }
    return report.str();
}

std::string format_text_move_plan(const planning::MovePlan& plan) {
    std::ostringstream report;
    report << "Move plan: " << (plan ? "ready" : "blocked") << '\n';
    report << "Read-only: " << (plan.read_only ? "yes" : "no") << '\n';
    report << "Source: " << plan.source_path.string() << '\n';
    report << "Target: " << plan.target_path.string() << '\n';
    report << "Symbol ID: " << plan.symbol_id << '\n';
    if (!plan.qualified_name.empty()) {
        report << "Callable: " << plan.qualified_name << '\n';
    }
    if (plan.definition.has_value()) {
        report << "Definition: lines " << plan.definition->begin_line << '-'
               << plan.definition->end_line << ", offsets " << plan.definition->begin_offset << '-'
               << plan.definition->end_offset << '\n';
    }
    if (!plan.blockers.empty()) {
        report << "Blockers: " << plan.blockers.size() << '\n';
        for (const auto& blocker : plan.blockers) {
            report << "- " << blocker_kind_name(blocker.kind);
            if (!blocker.detail.empty()) {
                report << ": " << blocker.detail;
            }
            report << '\n';
        }
    }
    if (!plan.steps.empty()) {
        report << "Planned steps: " << plan.steps.size() << '\n';
        for (std::size_t index = 0; index < plan.steps.size(); ++index) {
            report << index + 1 << ". " << step_name(plan.steps[index].kind) << '\n';
        }
    }
    return report.str();
}

} // namespace codesplit::reporting

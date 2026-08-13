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
    }
    return report.str();
}

} // namespace codesplit::reporting

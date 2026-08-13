#include "codesplit/reporting/json_report.hpp"

#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace codesplit::reporting {
namespace {

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto encoded = path.generic_u8string();
    return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::string escape_json_string(std::string_view value) {
    constexpr std::string_view hexadecimal_digits = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size());

    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20U) {
                escaped += "\\u00";
                escaped += hexadecimal_digits[character >> 4U];
                escaped += hexadecimal_digits[character & 0x0fU];
            } else {
                escaped += static_cast<char>(character);
            }
        }
    }

    return escaped;
}

std::string_view callable_kind_name(analysis::CallableKind kind) {
    switch (kind) {
    case analysis::CallableKind::free_function:
        return "free_function";
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
        return "unique_external";
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
        return "inventory_unavailable";
    case symbol_not_found:
        return "symbol_not_found";
    case source_target_collision:
        return "source_target_collision";
    case not_free_function:
        return "not_free_function";
    case callable_constraint:
        return "callable_constraint";
    case non_external_linkage:
        return "non_external_linkage";
    case outgoing_dependency:
        return "outgoing_dependency";
    case incoming_dependency_without_declaration:
        return "incoming_dependency_without_declaration";
    case declaration_include_unavailable:
        return "declaration_include_unavailable";
    case macro_dependency:
        return "macro_dependency";
    }
    return "unknown";
}

std::string_view step_name(planning::MovePlanStepKind kind) {
    using enum planning::MovePlanStepKind;
    switch (kind) {
    case copy_definition:
        return "copy_definition";
    case remove_definition:
        return "remove_definition";
    case validate_frontend:
        return "validate_frontend";
    case build_and_test:
        return "build_and_test";
    }
    return "unknown";
}

std::string_view dry_run_blocker_name(planning::MoveDryRunBlockerKind kind) {
    using enum planning::MoveDryRunBlockerKind;
    switch (kind) {
    case plan_blocked:
        return "plan_blocked";
    case source_read_failed:
        return "source_read_failed";
    case target_exists:
        return "target_exists";
    case invalid_source_range:
        return "invalid_source_range";
    case overlapping_replacements:
        return "overlapping_replacements";
    }
    return "unknown";
}

std::string_view apply_blocker_name(planning::MoveApplyBlockerKind kind) {
    using enum planning::MoveApplyBlockerKind;
    switch (kind) {
    case dry_run_blocked:
        return "dry_run_blocked";
    case invalid_replacement_set:
        return "invalid_replacement_set";
    case source_changed:
        return "source_changed";
    case staging_failed:
        return "staging_failed";
    case commit_failed:
        return "commit_failed";
    case validation_failed:
        return "validation_failed";
    case rollback_failed:
        return "rollback_failed";
    }
    return "unknown";
}

void append_constraints(std::ostringstream& report,
                        const std::vector<analysis::CallableConstraint>& constraints) {
    report << '[';
    for (std::size_t index = 0; index < constraints.size(); ++index) {
        if (index != 0) {
            report << ", ";
        }
        report << '"' << constraint_name(constraints[index]) << '"';
    }
    report << ']';
}

void append_strings(std::ostringstream& report, const std::vector<std::string>& values) {
    report << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            report << ", ";
        }
        report << '"' << escape_json_string(values[index]) << '"';
    }
    report << ']';
}

void append_diagnostics(std::ostringstream& report,
                        const std::vector<analysis::FrontendDiagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        report << "[]";
        return;
    }

    report << "[\n";
    for (std::size_t index = 0; index < diagnostics.size(); ++index) {
        const auto& diagnostic = diagnostics[index];
        report << "      {\n";
        report << "        \"severity\": \"" << diagnostic_severity_name(diagnostic.severity)
               << "\",\n";
        report << "        \"message\": \"" << escape_json_string(diagnostic.message) << "\",\n";
        if (diagnostic.path.empty()) {
            report << "        \"path\": null,\n";
        } else {
            report << "        \"path\": \"" << escape_json_string(path_to_utf8(diagnostic.path))
                   << "\",\n";
        }
        report << "        \"line\": " << diagnostic.line << ",\n";
        report << "        \"column\": " << diagnostic.column << "\n";
        report << "      }" << (index + 1 == diagnostics.size() ? "\n" : ",\n");
    }
    report << "    ]";
}

void append_dependencies(std::ostringstream& report,
                         const std::vector<analysis::CallableDependency>& dependencies) {
    if (dependencies.empty()) {
        report << "[]";
        return;
    }

    report << "[\n";
    for (std::size_t index = 0; index < dependencies.size(); ++index) {
        const auto& dependency = dependencies[index];
        report << "      {\n";
        report << "        \"kind\": \"" << dependency_kind_name(dependency.kind) << "\",\n";
        report << "        \"source_symbol_id\": \""
               << escape_json_string(dependency.source_symbol_id) << "\",\n";
        report << "        \"source_qualified_name\": \""
               << escape_json_string(dependency.source_qualified_name) << "\",\n";
        report << "        \"target_symbol_id\": \""
               << escape_json_string(dependency.target_symbol_id) << "\",\n";
        report << "        \"target_qualified_name\": \""
               << escape_json_string(dependency.target_qualified_name) << "\"\n";
        report << "      }" << (index + 1 == dependencies.size() ? "\n" : ",\n");
    }
    report << "    ]";
}

void append_includes(std::ostringstream& report,
                     const std::vector<analysis::IncludeDependency>& includes) {
    if (includes.empty()) {
        report << "[]";
        return;
    }

    report << "[\n";
    for (std::size_t index = 0; index < includes.size(); ++index) {
        const auto& include = includes[index];
        report << "      {\n";
        report << "        \"kind\": \"" << include_kind_name(include.kind) << "\",\n";
        report << "        \"written_name\": \"" << escape_json_string(include.written_name)
               << "\",\n";
        report << "        \"resolved_path\": \""
               << escape_json_string(path_to_utf8(include.resolved_path)) << "\",\n";
        report << "        \"origin\": {\n";
        report << "          \"path\": \"" << escape_json_string(path_to_utf8(include.origin.path))
               << "\",\n";
        report << "          \"begin_offset\": " << include.origin.begin_offset << ",\n";
        report << "          \"end_offset\": " << include.origin.end_offset << ",\n";
        report << "          \"begin_line\": " << include.origin.begin_line << ",\n";
        report << "          \"end_line\": " << include.origin.end_line << "\n";
        report << "        }\n";
        report << "      }" << (index + 1 == includes.size() ? "\n" : ",\n");
    }
    report << "    ]";
}

void append_macros(std::ostringstream& report,
                   const std::vector<analysis::MacroDependency>& macros) {
    if (macros.empty()) {
        report << "[]";
        return;
    }

    report << "[\n";
    for (std::size_t index = 0; index < macros.size(); ++index) {
        const auto& macro = macros[index];
        report << "      {\n";
        report << "        \"source_symbol_id\": \"" << escape_json_string(macro.source_symbol_id)
               << "\",\n";
        report << "        \"source_qualified_name\": \""
               << escape_json_string(macro.source_qualified_name) << "\",\n";
        report << "        \"macro_name\": \"" << escape_json_string(macro.macro_name) << "\",\n";
        report << "        \"definition\": ";
        if (macro.definition.has_value()) {
            report << "{\n";
            report << "          \"path\": \""
                   << escape_json_string(path_to_utf8(macro.definition->path)) << "\",\n";
            report << "          \"begin_offset\": " << macro.definition->begin_offset << ",\n";
            report << "          \"end_offset\": " << macro.definition->end_offset << ",\n";
            report << "          \"begin_line\": " << macro.definition->begin_line << ",\n";
            report << "          \"end_line\": " << macro.definition->end_line << "\n";
            report << "        },\n";
        } else {
            report << "null,\n";
        }
        report << "        \"expansions\": [\n";
        for (std::size_t expansion_index = 0; expansion_index < macro.expansions.size();
             ++expansion_index) {
            const auto& expansion = macro.expansions[expansion_index];
            report << "          {\n";
            report << "            \"path\": \"" << escape_json_string(path_to_utf8(expansion.path))
                   << "\",\n";
            report << "            \"begin_offset\": " << expansion.begin_offset << ",\n";
            report << "            \"end_offset\": " << expansion.end_offset << ",\n";
            report << "            \"begin_line\": " << expansion.begin_line << ",\n";
            report << "            \"end_line\": " << expansion.end_line << "\n";
            report << "          }"
                   << (expansion_index + 1 == macro.expansions.size() ? "\n" : ",\n");
        }
        report << "        ]\n";
        report << "      }" << (index + 1 == macros.size() ? "\n" : ",\n");
    }
    report << "    ]";
}

void append_source_range(std::ostringstream& report,
                         const std::optional<analysis::SourceRange>& source_range) {
    if (!source_range.has_value()) {
        report << "null";
        return;
    }

    report << "{\n";
    report << "          \"path\": \"" << escape_json_string(path_to_utf8(source_range->path))
           << "\",\n";
    report << "          \"begin_offset\": " << source_range->begin_offset << ",\n";
    report << "          \"end_offset\": " << source_range->end_offset << ",\n";
    report << "          \"begin_line\": " << source_range->begin_line << ",\n";
    report << "          \"end_line\": " << source_range->end_line << "\n";
    report << "        }";
}

void append_callable(std::ostringstream& report, const analysis::CallableDefinition& callable,
                     bool is_last) {
    report << "      {\n";
    report << "        \"kind\": \"" << callable_kind_name(callable.kind) << "\",\n";
    report << "        \"linkage\": \"" << linkage_name(callable.linkage) << "\",\n";
    report << "        \"in_anonymous_namespace\": "
           << (callable.in_anonymous_namespace ? "true" : "false") << ",\n";
    report << "        \"qualified_name\": \"" << escape_json_string(callable.qualified_name)
           << "\",\n";
    if (callable.symbol_id.empty()) {
        report << "        \"symbol_id\": null,\n";
    } else {
        report << "        \"symbol_id\": \"" << escape_json_string(callable.symbol_id) << "\",\n";
    }
    report << "        \"enclosing_namespaces\": ";
    append_strings(report, callable.enclosing_namespaces);
    report << ",\n";
    report << "        \"declaration\": ";
    append_source_range(report, callable.declaration);
    report << ",\n";
    report << "        \"owning_record\": ";
    append_source_range(report, callable.owning_record);
    report << ",\n";
    report << "        \"begin_offset\": " << callable.begin_offset << ",\n";
    report << "        \"end_offset\": " << callable.end_offset << ",\n";
    report << "        \"size_bytes\": " << callable.size_bytes << ",\n";
    report << "        \"begin_line\": " << callable.begin_line << ",\n";
    report << "        \"end_line\": " << callable.end_line << ",\n";
    report << "        \"constraints\": ";
    append_constraints(report, callable.constraints);
    report << "\n      }" << (is_last ? "\n" : ",\n");
}

} // namespace

std::string format_json_report(const analysis::SourceFileInfo& info, std::uintmax_t size_limit_kib,
                               const analysis::CallableInventoryResult& inventory) {
    std::ostringstream report;
    report << "{\n";
    report << "  \"file\": \"" << escape_json_string(path_to_utf8(info.path)) << "\",\n";
    report << "  \"size_bytes\": " << info.size_bytes << ",\n";
    report << "  \"line_count\": " << info.line_count << ",\n";
    report << "  \"size_limit_kib\": " << size_limit_kib << ",\n";
    report << "  \"exceeds_size_limit\": " << (info.exceeds_size_limit ? "true" : "false") << ",\n";
    report << "  \"compilation_command\": {\n";
    report << "    \"available\": " << (inventory.compilation ? "true" : "false") << ",\n";
    if (inventory.compilation) {
        report << "    \"working_directory\": \""
               << escape_json_string(path_to_utf8(inventory.compilation.command.working_directory))
               << "\",\n";
        report << "    \"error\": null\n";
    } else {
        report << "    \"working_directory\": null,\n";
        report << "    \"error\": \"" << escape_json_string(inventory.compilation.error) << "\"\n";
    }
    report << "  },\n";
    report << "  \"callable_inventory\": {\n";
    report << "    \"available\": " << (inventory ? "true" : "false") << ",\n";
    if (inventory) {
        report << "    \"error\": null,\n";
    } else {
        report << "    \"error\": \"" << escape_json_string(inventory.error) << "\",\n";
    }
    report << "    \"diagnostics\": ";
    append_diagnostics(report, inventory.diagnostics);
    report << ",\n";
    report << "    \"dependencies\": ";
    append_dependencies(report, inventory.dependencies);
    report << ",\n";
    report << "    \"includes\": ";
    append_includes(report, inventory.includes);
    report << ",\n";
    report << "    \"macros\": ";
    append_macros(report, inventory.macros);
    report << ",\n";
    report << "    \"definitions\": ";
    if (inventory.callables.empty()) {
        report << "[]\n";
    } else {
        report << "[\n";
        for (std::size_t index = 0; index < inventory.callables.size(); ++index) {
            append_callable(report, inventory.callables[index],
                            index + 1 == inventory.callables.size());
        }
        report << "    ]\n";
    }
    report << "  }\n";
    report << "}\n";
    return report.str();
}

std::string format_json_move_plan(const planning::MovePlan& plan) {
    std::ostringstream report;
    report << "{\n";
    report << "  \"status\": \"" << (plan ? "ready" : "blocked") << "\",\n";
    report << "  \"read_only\": " << (plan.read_only ? "true" : "false") << ",\n";
    report << "  \"source\": \"" << escape_json_string(path_to_utf8(plan.source_path)) << "\",\n";
    report << "  \"target\": \"" << escape_json_string(path_to_utf8(plan.target_path)) << "\",\n";
    report << "  \"symbol_id\": \"" << escape_json_string(plan.symbol_id) << "\",\n";
    report << "  \"qualified_name\": \"" << escape_json_string(plan.qualified_name) << "\",\n";
    report << "  \"enclosing_namespaces\": ";
    append_strings(report, plan.enclosing_namespaces);
    report << ",\n";
    report << "  \"declaration_include\": ";
    if (plan.declaration_include.has_value()) {
        report << "\"" << escape_json_string(plan.declaration_include->written_name) << "\",\n";
    } else {
        report << "null,\n";
    }
    report << "  \"definition\": ";
    if (plan.definition.has_value()) {
        report << "{\n";
        report << "    \"path\": \"" << escape_json_string(path_to_utf8(plan.definition->path))
               << "\",\n";
        report << "    \"begin_offset\": " << plan.definition->begin_offset << ",\n";
        report << "    \"end_offset\": " << plan.definition->end_offset << ",\n";
        report << "    \"begin_line\": " << plan.definition->begin_line << ",\n";
        report << "    \"end_line\": " << plan.definition->end_line << "\n";
        report << "  },\n";
    } else {
        report << "null,\n";
    }
    report << "  \"blockers\": [";
    if (!plan.blockers.empty()) {
        report << '\n';
        for (std::size_t index = 0; index < plan.blockers.size(); ++index) {
            const auto& blocker = plan.blockers[index];
            report << "    {\n";
            report << "      \"kind\": \"" << blocker_kind_name(blocker.kind) << "\",\n";
            report << "      \"detail\": \"" << escape_json_string(blocker.detail) << "\"\n";
            report << "    }" << (index + 1 == plan.blockers.size() ? "\n" : ",\n");
        }
        report << "  ],\n";
    } else {
        report << "],\n";
    }
    report << "  \"steps\": [";
    for (std::size_t index = 0; index < plan.steps.size(); ++index) {
        if (index != 0) {
            report << ", ";
        }
        report << '"' << step_name(plan.steps[index].kind) << '"';
    }
    report << "]\n";
    report << "}\n";
    return report.str();
}

std::string format_json_move_dry_run(const planning::MoveDryRun& dry_run) {
    std::ostringstream report;
    report << "{\n";
    report << "  \"status\": \"" << (dry_run ? "ready" : "blocked") << "\",\n";
    report << "  \"read_only\": " << (dry_run.read_only ? "true" : "false") << ",\n";
    report << "  \"source\": \"" << escape_json_string(path_to_utf8(dry_run.plan.source_path))
           << "\",\n";
    report << "  \"target\": \"" << escape_json_string(path_to_utf8(dry_run.plan.target_path))
           << "\",\n";
    report << "  \"symbol_id\": \"" << escape_json_string(dry_run.plan.symbol_id) << "\",\n";
    report << "  \"blockers\": [";
    if (!dry_run.blockers.empty()) {
        report << '\n';
        for (std::size_t index = 0; index < dry_run.blockers.size(); ++index) {
            const auto& blocker = dry_run.blockers[index];
            report << "    {\"kind\": \"" << dry_run_blocker_name(blocker.kind)
                   << "\", \"detail\": \"" << escape_json_string(blocker.detail) << "\"}"
                   << (index + 1 == dry_run.blockers.size() ? "\n" : ",\n");
        }
        report << "  ],\n";
    } else {
        report << "],\n";
    }
    report << "  \"replacements\": [";
    if (!dry_run.replacements.empty()) {
        report << '\n';
        for (std::size_t index = 0; index < dry_run.replacements.size(); ++index) {
            const auto& replacement = dry_run.replacements[index];
            report << "    {\n";
            report << "      \"path\": \"" << escape_json_string(path_to_utf8(replacement.path))
                   << "\",\n";
            report << "      \"begin_offset\": " << replacement.begin_offset << ",\n";
            report << "      \"end_offset\": " << replacement.end_offset << ",\n";
            report << "      \"replacement_text\": \""
                   << escape_json_string(replacement.replacement_text) << "\"\n";
            report << "    }" << (index + 1 == dry_run.replacements.size() ? "\n" : ",\n");
        }
        report << "  ]\n";
    } else {
        report << "]\n";
    }
    report << "}\n";
    return report.str();
}

std::string format_json_move_apply(const planning::MoveApplyResult& result) {
    std::ostringstream report;
    report << "{\n";
    report << "  \"status\": \"" << (result ? "applied" : "blocked") << "\",\n";
    report << "  \"applied\": " << (result.applied ? "true" : "false") << ",\n";
    report << "  \"validated\": " << (result.validated ? "true" : "false") << ",\n";
    report << "  \"rolled_back\": " << (result.rolled_back ? "true" : "false") << ",\n";
    report << "  \"source\": \""
           << escape_json_string(path_to_utf8(result.dry_run.plan.source_path)) << "\",\n";
    report << "  \"target\": \""
           << escape_json_string(path_to_utf8(result.dry_run.plan.target_path)) << "\",\n";
    report << "  \"symbol_id\": \"" << escape_json_string(result.dry_run.plan.symbol_id) << "\",\n";
    report << "  \"blockers\": [";
    for (std::size_t index = 0; index < result.blockers.size(); ++index) {
        const auto& blocker = result.blockers[index];
        report << (index == 0 ? "\n" : ",\n");
        report << "    {\"kind\": \"" << apply_blocker_name(blocker.kind) << "\", \"detail\": \""
               << escape_json_string(blocker.detail) << "\"}";
    }
    report << (result.blockers.empty() ? "],\n" : "\n  ],\n");
    report << "  \"warnings\": [";
    for (std::size_t index = 0; index < result.warnings.size(); ++index) {
        if (index != 0) {
            report << ", ";
        }
        report << '"' << escape_json_string(result.warnings[index]) << '"';
    }
    report << "]\n";
    report << "}\n";
    return report.str();
}

} // namespace codesplit::reporting

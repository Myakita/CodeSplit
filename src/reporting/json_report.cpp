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
    report << "        \"qualified_name\": \"" << escape_json_string(callable.qualified_name)
           << "\",\n";
    if (callable.symbol_id.empty()) {
        report << "        \"symbol_id\": null,\n";
    } else {
        report << "        \"symbol_id\": \"" << escape_json_string(callable.symbol_id) << "\",\n";
    }
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

} // namespace codesplit::reporting

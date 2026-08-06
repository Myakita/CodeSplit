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
    if (inventory) {
        report << "Callable definitions: " << inventory.callables.size() << '\n';
        for (const auto& callable : inventory.callables) {
            append_callable(report, callable);
        }
    }
    return report.str();
}

} // namespace codesplit::reporting

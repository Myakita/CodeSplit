#include "codesplit/reporting/text_report.hpp"

#include <sstream>

namespace codesplit::reporting {

std::string format_text_report(const analysis::SourceFileInfo& info, std::uintmax_t size_limit_kib,
                               const analysis::CompilationCommandResult& compilation) {
    std::ostringstream report;
    report << "File: " << info.path.string() << '\n';
    report << "Size: " << info.size_bytes << " bytes\n";
    report << "Lines: " << info.line_count << '\n';
    report << "Exceeds " << size_limit_kib << " KiB: " << (info.exceeds_size_limit ? "yes" : "no")
           << '\n';
    report << "Compilation command: " << (compilation ? "available" : "unavailable") << '\n';
    if (!compilation) {
        report << "Reason: " << compilation.error << '\n';
    }
    return report.str();
}

} // namespace codesplit::reporting

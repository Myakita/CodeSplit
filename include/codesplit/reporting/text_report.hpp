#pragma once

#include "codesplit/analysis/compilation_database.hpp"
#include "codesplit/analysis/source_file.hpp"

#include <cstdint>
#include <string>

namespace codesplit::reporting {

[[nodiscard]] std::string format_text_report(const analysis::SourceFileInfo& info,
                                             std::uintmax_t size_limit_kib,
                                             const analysis::CompilationCommandResult& compilation);

} // namespace codesplit::reporting

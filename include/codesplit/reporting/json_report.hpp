#pragma once

#include "codesplit/analysis/callable_inventory.hpp"
#include "codesplit/analysis/source_file.hpp"

#include <cstdint>
#include <string>

namespace codesplit::reporting {

[[nodiscard]] std::string format_json_report(const analysis::SourceFileInfo& info,
                                             std::uintmax_t size_limit_kib,
                                             const analysis::CallableInventoryResult& inventory);

} // namespace codesplit::reporting

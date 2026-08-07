#pragma once

#include "codesplit/analysis/compilation_database.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codesplit::analysis {

enum class CallableKind { free_function, method };

enum class CallableConstraint {
    macro_expansion,
    source_range_unavailable,
    exceeds_size_limit,
};

struct SourceRange {
    std::filesystem::path path;
    std::uintmax_t begin_offset = 0;
    std::uintmax_t end_offset = 0;
    std::uintmax_t begin_line = 0;
    std::uintmax_t end_line = 0;
};

struct CallableDefinition {
    CallableKind kind = CallableKind::free_function;
    std::string qualified_name;
    std::string symbol_id;
    std::optional<SourceRange> declaration;
    std::optional<SourceRange> owning_record;
    std::uintmax_t begin_offset = 0;
    std::uintmax_t end_offset = 0;
    std::uintmax_t size_bytes = 0;
    std::uintmax_t begin_line = 0;
    std::uintmax_t end_line = 0;
    std::vector<CallableConstraint> constraints;
};

struct CallableInventoryResult {
    CompilationCommandResult compilation;
    std::vector<CallableDefinition> callables;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] CallableInventoryResult inventory_callables(const std::filesystem::path& build_path,
                                                          const std::filesystem::path& source_path,
                                                          std::uintmax_t size_limit_bytes);

} // namespace codesplit::analysis

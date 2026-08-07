#pragma once

#include "codesplit/analysis/compilation_database.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codesplit::analysis {

enum class CallableKind { free_function, method };

enum class FrontendDiagnosticSeverity { note, remark, warning, error, fatal };

enum class CallableDependencyKind { direct_call, type_reference, global_read, global_write };

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

struct FrontendDiagnostic {
    FrontendDiagnosticSeverity severity = FrontendDiagnosticSeverity::note;
    std::string message;
    std::filesystem::path path;
    std::uintmax_t line = 0;
    std::uintmax_t column = 0;
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

struct CallableDependency {
    CallableDependencyKind kind = CallableDependencyKind::direct_call;
    std::string source_symbol_id;
    std::string source_qualified_name;
    std::string target_symbol_id;
    std::string target_qualified_name;
};

struct CallableInventoryResult {
    CompilationCommandResult compilation;
    std::vector<CallableDefinition> callables;
    std::vector<CallableDependency> dependencies;
    std::vector<FrontendDiagnostic> diagnostics;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] CallableInventoryResult inventory_callables(const std::filesystem::path& build_path,
                                                          const std::filesystem::path& source_path,
                                                          std::uintmax_t size_limit_bytes);

} // namespace codesplit::analysis

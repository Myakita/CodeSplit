#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace codesplit::analysis {

inline constexpr std::uintmax_t default_size_limit_bytes = 100U * 1024U;

struct SourceFileInfo {
    std::filesystem::path path;
    std::uintmax_t size_bytes{0};
    std::uint64_t line_count{0};
    bool exceeds_size_limit{false};
};

struct SourceFileAnalysisResult {
    SourceFileInfo info;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] SourceFileAnalysisResult
analyze_source_file(const std::filesystem::path& path,
                    std::uintmax_t size_limit_bytes = default_size_limit_bytes);

} // namespace codesplit::analysis

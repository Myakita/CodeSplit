#include "codesplit/analysis/source_file.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>

namespace codesplit::analysis {
namespace {

constexpr std::size_t read_buffer_size = 64U * 1024U;

} // namespace

SourceFileAnalysisResult analyze_source_file(const std::filesystem::path& path,
                                             std::uintmax_t size_limit_bytes) {
    SourceFileAnalysisResult result;
    result.info.path = path;

    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(path, filesystem_error)) {
        result.error = "Input path is not a regular file: " + path.string();
        return result;
    }

    result.info.size_bytes = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        result.error = "Unable to determine file size: " + path.string();
        return result;
    }

    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        result.error = "Unable to open input file: " + path.string();
        return result;
    }

    std::array<char, read_buffer_size> buffer{};
    bool has_content = false;
    char last_character = '\0';

    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto characters_read = stream.gcount();
        if (characters_read == 0) {
            continue;
        }

        has_content = true;
        last_character = buffer[static_cast<std::size_t>(characters_read - 1)];
        result.info.line_count += static_cast<std::uint64_t>(
            std::count(buffer.begin(), buffer.begin() + characters_read, '\n'));
    }

    if (!stream.eof()) {
        result.error = "Unable to read input file: " + path.string();
        return result;
    }

    if (has_content && last_character != '\n') {
        ++result.info.line_count;
    }

    result.info.exceeds_size_limit = result.info.size_bytes > size_limit_bytes;
    return result;
}

} // namespace codesplit::analysis

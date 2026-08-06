#include "codesplit/reporting/json_report.hpp"

#include <sstream>
#include <string_view>

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

} // namespace

std::string format_json_report(const analysis::SourceFileInfo& info, std::uintmax_t size_limit_kib,
                               const analysis::CompilationCommandResult& compilation) {
    std::ostringstream report;
    report << "{\n";
    report << "  \"file\": \"" << escape_json_string(path_to_utf8(info.path)) << "\",\n";
    report << "  \"size_bytes\": " << info.size_bytes << ",\n";
    report << "  \"line_count\": " << info.line_count << ",\n";
    report << "  \"size_limit_kib\": " << size_limit_kib << ",\n";
    report << "  \"exceeds_size_limit\": " << (info.exceeds_size_limit ? "true" : "false") << ",\n";
    report << "  \"compilation_command\": {\n";
    report << "    \"available\": " << (compilation ? "true" : "false") << ",\n";
    if (compilation) {
        report << "    \"working_directory\": \""
               << escape_json_string(path_to_utf8(compilation.command.working_directory))
               << "\",\n";
        report << "    \"error\": null\n";
    } else {
        report << "    \"working_directory\": null,\n";
        report << "    \"error\": \"" << escape_json_string(compilation.error) << "\"\n";
    }
    report << "  }\n";
    report << "}\n";
    return report.str();
}

} // namespace codesplit::reporting

#include "codesplit/planning/move_dry_run.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>

namespace codesplit::planning {
namespace {

std::filesystem::path normalized_absolute_path(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    return (error ? path : absolute).lexically_normal();
}

void add_blocker(MoveDryRun& dry_run, MoveDryRunBlockerKind kind, std::string detail) {
    dry_run.blockers.push_back({.kind = kind, .detail = std::move(detail)});
}

std::string_view line_ending_for(const std::string& source_text, std::size_t end_offset) {
    if (source_text.compare(end_offset, 2, "\r\n") == 0 ||
        source_text.find("\r\n") != std::string::npos) {
        return "\r\n";
    }
    return "\n";
}

std::string target_text_for(const MovePlan& plan, std::string_view definition,
                            std::string_view line_ending) {
    std::string target_text;
    if (plan.declaration_include.has_value()) {
        const auto angled = plan.declaration_include->kind == analysis::IncludeKind::angled;
        target_text += "#include ";
        target_text += angled ? '<' : '"';
        target_text += plan.declaration_include->written_name;
        target_text += angled ? '>' : '"';
        target_text += line_ending;
        target_text += line_ending;
    }
    for (const auto& namespace_name : plan.enclosing_namespaces) {
        target_text += "namespace " + namespace_name + " {";
        target_text += line_ending;
    }
    if (!plan.enclosing_namespaces.empty()) {
        target_text += line_ending;
    }
    target_text += definition;
    target_text += line_ending;
    for (auto namespace_name = plan.enclosing_namespaces.rbegin();
         namespace_name != plan.enclosing_namespaces.rend(); ++namespace_name) {
        target_text += line_ending;
        target_text += "} // namespace " + *namespace_name;
    }
    if (!plan.enclosing_namespaces.empty()) {
        target_text += line_ending;
    }
    return target_text;
}

std::string joined_arguments(const std::vector<std::string>& parameter_names) {
    std::ostringstream arguments;
    for (std::size_t index = 0; index < parameter_names.size(); ++index) {
        if (index != 0) {
            arguments << ", ";
        }
        arguments << parameter_names[index];
    }
    return arguments.str();
}

std::string trim_trailing_whitespace(std::string text) {
    const auto last_character = text.find_last_not_of(" \t\r\n");
    text.resize(last_character == std::string::npos ? 0 : last_character + 1);
    return text;
}

} // namespace

bool replacements_overlap(const TextReplacement& left, const TextReplacement& right) {
    if (normalized_absolute_path(left.path) != normalized_absolute_path(right.path)) {
        return false;
    }
    if (left.begin_offset == left.end_offset && right.begin_offset == right.end_offset) {
        return left.begin_offset == right.begin_offset;
    }
    return left.begin_offset < right.end_offset && right.begin_offset < left.end_offset;
}

MoveDryRun draft_callable_move(const MovePlan& plan) {
    MoveDryRun dry_run{.plan = plan};
    if (!plan) {
        add_blocker(dry_run, MoveDryRunBlockerKind::plan_blocked, "move plan contains blockers");
        return dry_run;
    }
    if (!plan.definition.has_value() || !plan.body.has_value()) {
        add_blocker(dry_run, MoveDryRunBlockerKind::invalid_source_range,
                    "move plan has no definition or body range");
        return dry_run;
    }

    std::error_code target_error;
    if (std::filesystem::exists(plan.target_path, target_error)) {
        add_blocker(dry_run, MoveDryRunBlockerKind::target_exists, plan.target_path.string());
        return dry_run;
    }

    std::ifstream source{plan.source_path, std::ios::binary};
    if (!source) {
        add_blocker(dry_run, MoveDryRunBlockerKind::source_read_failed, plan.source_path.string());
        return dry_run;
    }
    const std::string source_text{std::istreambuf_iterator<char>{source},
                                  std::istreambuf_iterator<char>{}};
    const auto begin = plan.definition->begin_offset;
    const auto end = plan.definition->end_offset;
    const auto body_begin = plan.body->begin_offset;
    const auto name_begin = plan.name_offset;
    if (begin >= end || end > source_text.size() || body_begin <= begin || body_begin >= end ||
        name_begin < begin || name_begin + plan.callable_name.size() > body_begin ||
        plan.callable_name.empty() || plan.implementation_name.empty()) {
        add_blocker(dry_run, MoveDryRunBlockerKind::invalid_source_range,
                    std::to_string(begin) + "-" + std::to_string(end));
        return dry_run;
    }
    if (source_text.compare(static_cast<std::size_t>(name_begin), plan.callable_name.size(),
                            plan.callable_name) != 0) {
        add_blocker(dry_run, MoveDryRunBlockerKind::invalid_source_range,
                    "callable name does not match its source range");
        return dry_run;
    }

    const auto definition =
        source_text.substr(static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin));
    const auto line_ending = line_ending_for(source_text, static_cast<std::size_t>(end));
    const auto relative_name = static_cast<std::size_t>(name_begin - begin);
    const auto relative_body = static_cast<std::size_t>(body_begin - begin);
    auto implementation_definition = definition;
    implementation_definition.replace(relative_name, plan.callable_name.size(),
                                      plan.implementation_name);
    auto implementation_declaration = definition.substr(0, relative_body);
    implementation_declaration.replace(relative_name, plan.callable_name.size(),
                                       plan.implementation_name);
    implementation_declaration = trim_trailing_whitespace(std::move(implementation_declaration));
    implementation_declaration += ';';
    auto wrapper = definition.substr(0, relative_body);
    wrapper += '{';
    wrapper += line_ending;
    wrapper += "    ";
    if (!plan.returns_void) {
        wrapper += "return ";
    }
    wrapper += plan.implementation_name + '(' + joined_arguments(plan.parameter_names) + ");";
    wrapper += line_ending;
    wrapper += '}';
    auto source_replacement = implementation_declaration;
    source_replacement += line_ending;
    source_replacement += line_ending;
    source_replacement += wrapper;
    auto target_text = target_text_for(plan, implementation_definition, line_ending);
    dry_run.replacements = {
        {.path = plan.source_path,
         .begin_offset = begin,
         .end_offset = end,
         .expected_text = definition,
         .replacement_text = std::move(source_replacement)},
        {.path = plan.target_path,
         .begin_offset = 0,
         .end_offset = 0,
         .replacement_text = std::move(target_text)},
    };
    if (replacements_overlap(dry_run.replacements[0], dry_run.replacements[1])) {
        dry_run.replacements.clear();
        add_blocker(dry_run, MoveDryRunBlockerKind::overlapping_replacements,
                    "source removal overlaps target insertion");
    }
    return dry_run;
}

} // namespace codesplit::planning

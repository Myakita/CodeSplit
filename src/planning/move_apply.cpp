#include "codesplit/planning/move_apply.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>

namespace codesplit::planning {
namespace {

void add_blocker(MoveApplyResult& result, MoveApplyBlockerKind kind, std::string detail) {
    result.blockers.push_back({.kind = kind, .detail = std::move(detail)});
}

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

bool write_file(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(output);
}

std::filesystem::path sibling_path(const std::filesystem::path& path, std::string_view suffix) {
    auto result = path;
    result += suffix;
    return result;
}

void remove_if_present(const std::filesystem::path& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

} // namespace

MoveApplyResult apply_callable_move(const MoveDryRun& dry_run) {
    MoveApplyResult result{.dry_run = dry_run};
    if (!dry_run) {
        add_blocker(result, MoveApplyBlockerKind::dry_run_blocked, "dry run contains blockers");
        return result;
    }
    if (dry_run.replacements.size() != 2) {
        add_blocker(result, MoveApplyBlockerKind::invalid_replacement_set,
                    "exactly two replacements are required");
        return result;
    }

    const auto& removal = dry_run.replacements[0];
    const auto& insertion = dry_run.replacements[1];
    if (removal.path != dry_run.plan.source_path || insertion.path != dry_run.plan.target_path ||
        !removal.replacement_text.empty() || insertion.begin_offset != 0 ||
        insertion.end_offset != 0) {
        add_blocker(result, MoveApplyBlockerKind::invalid_replacement_set,
                    "replacement roles do not match the move plan");
        return result;
    }

    const auto source_text = read_file(removal.path);
    if (!source_text.has_value() || removal.begin_offset > removal.end_offset ||
        removal.end_offset > source_text->size()) {
        add_blocker(result, MoveApplyBlockerKind::source_changed,
                    "source file or replacement range changed after dry run");
        return result;
    }
    const auto current_text =
        source_text->substr(static_cast<std::size_t>(removal.begin_offset),
                            static_cast<std::size_t>(removal.end_offset - removal.begin_offset));
    if (current_text != removal.expected_text) {
        add_blocker(result, MoveApplyBlockerKind::source_changed,
                    "source range no longer matches the dry run");
        return result;
    }

    std::error_code existence_error;
    if (std::filesystem::exists(insertion.path, existence_error)) {
        add_blocker(result, MoveApplyBlockerKind::source_changed,
                    "target file appeared after dry run");
        return result;
    }

    auto updated_source = *source_text;
    updated_source.replace(static_cast<std::size_t>(removal.begin_offset),
                           static_cast<std::size_t>(removal.end_offset - removal.begin_offset),
                           removal.replacement_text);

    const auto source_stage = sibling_path(removal.path, ".codesplit.tmp");
    const auto target_stage = sibling_path(insertion.path, ".codesplit.tmp");
    const auto source_backup = sibling_path(removal.path, ".codesplit.bak");
    if (std::filesystem::exists(source_stage) || std::filesystem::exists(target_stage) ||
        std::filesystem::exists(source_backup)) {
        add_blocker(result, MoveApplyBlockerKind::staging_failed,
                    "a CodeSplit staging or backup file already exists");
        return result;
    }
    if (!write_file(source_stage, updated_source) ||
        !write_file(target_stage, insertion.replacement_text)) {
        remove_if_present(source_stage);
        remove_if_present(target_stage);
        add_blocker(result, MoveApplyBlockerKind::staging_failed, "could not write staged files");
        return result;
    }

    std::error_code error;
    std::filesystem::rename(removal.path, source_backup, error);
    if (error) {
        remove_if_present(source_stage);
        remove_if_present(target_stage);
        add_blocker(result, MoveApplyBlockerKind::commit_failed, error.message());
        return result;
    }
    std::filesystem::rename(source_stage, removal.path, error);
    if (!error) {
        std::filesystem::rename(target_stage, insertion.path, error);
    }
    if (error) {
        remove_if_present(removal.path);
        remove_if_present(insertion.path);
        remove_if_present(source_stage);
        remove_if_present(target_stage);
        std::error_code rollback_error;
        std::filesystem::rename(source_backup, removal.path, rollback_error);
        result.rolled_back = !rollback_error;
        add_blocker(result,
                    rollback_error ? MoveApplyBlockerKind::rollback_failed
                                   : MoveApplyBlockerKind::commit_failed,
                    rollback_error ? rollback_error.message() : error.message());
        return result;
    }

    result.applied = true;
    if (!std::filesystem::remove(source_backup, error) || error) {
        result.warnings.push_back("source backup remains at " + source_backup.string());
    }
    return result;
}

} // namespace codesplit::planning

#include "codesplit/planning/move_apply.hpp"

#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

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

struct StagedReplacement {
    const TextReplacement* replacement = nullptr;
    std::filesystem::path stage_path;
    std::filesystem::path backup_path;
    bool had_original = false;
    bool backed_up = false;
    bool installed = false;
};

bool rollback(std::vector<StagedReplacement>& staged) {
    bool success = true;
    for (auto& entry : staged) {
        if (entry.installed) {
            remove_if_present(entry.replacement->path);
        }
        remove_if_present(entry.stage_path);
    }
    for (auto& entry : staged) {
        if (!entry.backed_up) {
            continue;
        }
        std::error_code error;
        std::filesystem::rename(entry.backup_path, entry.replacement->path, error);
        success = success && !error;
    }
    return success;
}

MoveApplyResult apply_callable_move(const MoveDryRun& dry_run, const MoveValidator& validator,
                                    const MoveRollbackAction& rollback_action) {
    MoveApplyResult result{.dry_run = dry_run};
    if (!dry_run) {
        add_blocker(result, MoveApplyBlockerKind::dry_run_blocked, "dry run contains blockers");
        return result;
    }
    if (dry_run.replacements.size() < 2) {
        add_blocker(result, MoveApplyBlockerKind::invalid_replacement_set,
                    "source removal and target insertion are required");
        return result;
    }

    const auto& removal = dry_run.replacements[0];
    const auto& insertion = dry_run.replacements[1];
    if (removal.path != dry_run.plan.source_path || insertion.path != dry_run.plan.target_path ||
        insertion.begin_offset != 0 || insertion.end_offset != 0) {
        add_blocker(result, MoveApplyBlockerKind::invalid_replacement_set,
                    "source rewrite and target insertion do not match the move plan");
        return result;
    }

    std::vector<StagedReplacement> staged;
    staged.reserve(dry_run.replacements.size());
    for (const auto& replacement : dry_run.replacements) {
        const auto current = read_file(replacement.path);
        if (replacement.path == insertion.path && current.has_value()) {
            add_blocker(result, MoveApplyBlockerKind::source_changed,
                        "target file appeared after dry run");
            return result;
        }
        if (!current.has_value() && replacement.path != insertion.path) {
            add_blocker(result, MoveApplyBlockerKind::source_changed,
                        "an input file disappeared after dry run");
            return result;
        }
        const std::string current_text = current.value_or(std::string{});
        if (replacement.begin_offset > replacement.end_offset ||
            replacement.end_offset > current_text.size() ||
            current_text.substr(
                static_cast<std::size_t>(replacement.begin_offset),
                static_cast<std::size_t>(replacement.end_offset - replacement.begin_offset)) !=
                replacement.expected_text) {
            add_blocker(result, MoveApplyBlockerKind::source_changed,
                        "replacement input no longer matches the dry run");
            return result;
        }
        auto updated = current_text;
        updated.replace(static_cast<std::size_t>(replacement.begin_offset),
                        static_cast<std::size_t>(replacement.end_offset - replacement.begin_offset),
                        replacement.replacement_text);
        StagedReplacement entry{
            .replacement = &replacement,
            .stage_path = sibling_path(replacement.path, ".codesplit.tmp"),
            .backup_path = sibling_path(replacement.path, ".codesplit.bak"),
            .had_original = current.has_value(),
        };
        if (std::filesystem::exists(entry.stage_path) ||
            std::filesystem::exists(entry.backup_path) || !write_file(entry.stage_path, updated)) {
            staged.push_back(std::move(entry));
            rollback(staged);
            add_blocker(result, MoveApplyBlockerKind::staging_failed,
                        "could not prepare all staged files");
            return result;
        }
        staged.push_back(std::move(entry));
    }

    std::error_code error;
    for (auto& entry : staged) {
        if (!entry.had_original) {
            continue;
        }
        std::filesystem::rename(entry.replacement->path, entry.backup_path, error);
        if (error) {
            const auto rollback_succeeded = rollback(staged);
            result.rolled_back = rollback_succeeded;
            add_blocker(result,
                        rollback_succeeded ? MoveApplyBlockerKind::commit_failed
                                           : MoveApplyBlockerKind::rollback_failed,
                        error.message());
            return result;
        }
        entry.backed_up = true;
    }
    for (auto& entry : staged) {
        std::filesystem::rename(entry.stage_path, entry.replacement->path, error);
        if (error) {
            const auto rollback_succeeded = rollback(staged);
            result.rolled_back = rollback_succeeded;
            add_blocker(result,
                        rollback_succeeded ? MoveApplyBlockerKind::commit_failed
                                           : MoveApplyBlockerKind::rollback_failed,
                        error.message());
            return result;
        }
        entry.installed = true;
    }

    if (validator) {
        MoveValidationResult validation;
        try {
            validation = validator(removal.path, insertion.path);
        } catch (const std::exception& exception) {
            validation.detail = exception.what();
        }
        if (!validation.success) {
            result.rolled_back = rollback(staged);
            if (result.rolled_back && rollback_action) {
                const auto cleanup = rollback_action();
                result.rolled_back = cleanup.success;
                if (!cleanup.success) {
                    validation.detail += "; build graph restore failed: " + cleanup.detail;
                }
            }
            add_blocker(result,
                        result.rolled_back ? MoveApplyBlockerKind::validation_failed
                                           : MoveApplyBlockerKind::rollback_failed,
                        validation.detail);
            return result;
        }
        result.validated = true;
    }

    result.applied = true;
    for (const auto& entry : staged) {
        if (!entry.backed_up) {
            continue;
        }
        error.clear();
        if (!std::filesystem::remove(entry.backup_path, error) || error) {
            result.warnings.push_back("backup remains at " + entry.backup_path.string());
        }
    }
    return result;
}

} // namespace codesplit::planning

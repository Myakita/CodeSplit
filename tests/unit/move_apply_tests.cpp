#include "codesplit/planning/move_apply.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() / "codesplit-move-apply-tests";
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

codesplit::planning::MoveDryRun create_dry_run(const std::filesystem::path& source,
                                               const std::filesystem::path& target) {
    const std::string contents = "int retained = 1;\n\nint isolated() { return 2; }\n";
    std::ofstream{source, std::ios::binary} << contents;
    const auto begin = contents.find("int isolated");
    const auto end = contents.find('}', begin) + 1;
    codesplit::planning::MovePlan plan{
        .source_path = source,
        .target_path = target,
        .symbol_id = "c:@F@isolated#",
        .qualified_name = "isolated",
        .definition =
            codesplit::analysis::SourceRange{
                .path = source,
                .begin_offset = begin,
                .end_offset = end,
                .begin_line = 3,
                .end_line = 3,
            },
        .steps = {{.kind = codesplit::planning::MovePlanStepKind::copy_definition}},
    };
    return codesplit::planning::draft_callable_move(plan);
}

void applies_staged_files_and_removes_backup() {
    TemporaryDirectory directory;
    const auto source = directory.path() / "source.cpp";
    const auto target = directory.path() / "target.cpp";
    const auto dry_run = create_dry_run(source, target);

    const auto result = codesplit::planning::apply_callable_move(dry_run);

    expect(static_cast<bool>(result), "valid dry run should be applied");
    expect(result.applied, "result should record successful application");
    expect(read_file(source) == "int retained = 1;\n\n\n",
           "source should retain all text outside the definition range");
    expect(read_file(target) == "int isolated() { return 2; }\n",
           "target should contain the moved definition");
    expect(!std::filesystem::exists(source.string() + ".codesplit.tmp"),
           "source staging file should be removed");
    expect(!std::filesystem::exists(target.string() + ".codesplit.tmp"),
           "target staging file should be removed");
    expect(!std::filesystem::exists(source.string() + ".codesplit.bak"),
           "source backup should be removed after commit");
}

void rejects_source_changed_after_dry_run() {
    TemporaryDirectory directory;
    const auto source = directory.path() / "source.cpp";
    const auto target = directory.path() / "target.cpp";
    const auto dry_run = create_dry_run(source, target);
    std::ofstream{source, std::ios::binary | std::ios::trunc}
        << "int retained = 1;\n\nint changed() { return 3; }\n";
    const auto changed_contents = read_file(source);

    const auto result = codesplit::planning::apply_callable_move(dry_run);

    expect(!result, "changed source should block application");
    expect(result.blockers.size() == 1, "changed source should produce one blocker");
    expect(result.blockers[0].kind == codesplit::planning::MoveApplyBlockerKind::source_changed,
           "blocker should identify stale dry-run input");
    expect(read_file(source) == changed_contents, "blocked application should preserve source");
    expect(!std::filesystem::exists(target), "blocked application should not create target");
}

void rejects_blocked_dry_run() {
    codesplit::planning::MoveDryRun dry_run{
        .blockers = {{.kind = codesplit::planning::MoveDryRunBlockerKind::target_exists}},
    };

    const auto result = codesplit::planning::apply_callable_move(dry_run);

    expect(!result, "blocked dry run should not be applied");
    expect(result.blockers[0].kind == codesplit::planning::MoveApplyBlockerKind::dry_run_blocked,
           "application should preserve the dry-run safety boundary");
}

void rolls_back_failed_validation() {
    TemporaryDirectory directory;
    const auto source = directory.path() / "source.cpp";
    const auto target = directory.path() / "target.cpp";
    const auto dry_run = create_dry_run(source, target);
    const auto original_source = read_file(source);

    const auto result = codesplit::planning::apply_callable_move(dry_run, [](const auto&,
                                                                             const auto&) {
        return codesplit::planning::MoveValidationResult{.detail = "synthetic frontend failure"};
    });

    expect(!result, "failed validation should reject application");
    expect(result.rolled_back, "failed validation should restore the source backup");
    expect(result.blockers[0].kind == codesplit::planning::MoveApplyBlockerKind::validation_failed,
           "result should identify validation failure");
    expect(read_file(source) == original_source, "rollback should restore exact source bytes");
    expect(!std::filesystem::exists(target), "rollback should remove generated target");
}

} // namespace

int main() {
    applies_staged_files_and_removes_backup();
    rejects_source_changed_after_dry_run();
    rejects_blocked_dry_run();
    rolls_back_failed_validation();
}

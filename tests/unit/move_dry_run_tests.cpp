#include "codesplit/planning/move_dry_run.hpp"

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

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() / "codesplit-move-dry-run-tests";
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

codesplit::planning::MovePlan ready_plan(const std::filesystem::path& source,
                                         const std::filesystem::path& target) {
    return {
        .source_path = source,
        .target_path = target,
        .symbol_id = "c:@F@isolated#",
        .qualified_name = "isolated",
        .definition =
            codesplit::analysis::SourceRange{
                .path = source,
                .begin_offset = 7,
                .end_offset = 35,
                .begin_line = 3,
                .end_line = 5,
            },
        .steps = {{.kind = codesplit::planning::MovePlanStepKind::copy_definition}},
    };
}

void drafts_two_non_overlapping_replacements() {
    TemporaryDirectory directory;
    const auto source = directory.path() / "source.cpp";
    const auto target = directory.path() / "target.cpp";
    const std::string contents = "int a;\nint isolated() {\n    return 1;\n}\n";
    std::ofstream{source, std::ios::binary} << contents;

    auto plan = ready_plan(source, target);
    plan.definition->end_offset = contents.find("}\n") + 1;
    const auto dry_run = codesplit::planning::draft_callable_move(plan);

    expect(static_cast<bool>(dry_run), "valid plan should produce a dry run");
    expect(dry_run.read_only, "dry run should be explicitly read-only");
    expect(dry_run.replacements.size() == 2, "dry run should contain removal and insertion");
    expect(dry_run.replacements[0].replacement_text.empty(), "source range should be removed");
    expect(dry_run.replacements[1].replacement_text == "int isolated() {\n    return 1;\n}\n",
           "target insertion should contain the exact definition and one trailing newline");
    expect(!codesplit::planning::replacements_overlap(dry_run.replacements[0],
                                                      dry_run.replacements[1]),
           "replacements in different files should not overlap");
    expect(std::filesystem::exists(source), "source should remain present");
    expect(!std::filesystem::exists(target), "target should not be created");
}

void rejects_existing_target() {
    TemporaryDirectory directory;
    const auto source = directory.path() / "source.cpp";
    const auto target = directory.path() / "target.cpp";
    std::ofstream{source} << "int isolated() { return 1; }\n";
    std::ofstream{target} << "existing\n";
    auto plan = ready_plan(source, target);
    plan.definition->begin_offset = 0;
    plan.definition->end_offset = 28;

    const auto dry_run = codesplit::planning::draft_callable_move(plan);

    expect(!dry_run, "existing target should block the initial dry-run slice");
    expect(dry_run.blockers.size() == 1, "existing target should produce one blocker");
    expect(dry_run.blockers[0].kind == codesplit::planning::MoveDryRunBlockerKind::target_exists,
           "blocker should identify the existing target");
    expect(dry_run.replacements.empty(), "blocked dry run should contain no replacements");
}

void detects_half_open_range_overlap() {
    using codesplit::planning::TextReplacement;
    const TextReplacement first{.path = "source.cpp", .begin_offset = 10, .end_offset = 20};
    const TextReplacement touching{.path = "source.cpp", .begin_offset = 20, .end_offset = 25};
    const TextReplacement overlapping{.path = "source.cpp", .begin_offset = 19, .end_offset = 25};

    expect(!codesplit::planning::replacements_overlap(first, touching),
           "touching half-open ranges should not overlap");
    expect(codesplit::planning::replacements_overlap(first, overlapping),
           "intersecting half-open ranges should overlap");
}

} // namespace

int main() {
    drafts_two_non_overlapping_replacements();
    rejects_existing_target();
    detects_half_open_range_overlap();
}

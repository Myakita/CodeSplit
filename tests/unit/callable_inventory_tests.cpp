#include "codesplit/analysis/callable_inventory.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failure_count = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failure_count;
    }
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto encoded = path.generic_u8string();
    return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

bool has_constraint(const codesplit::analysis::CallableDefinition& callable,
                    codesplit::analysis::CallableConstraint constraint) {
    return std::ranges::find(callable.constraints, constraint) != callable.constraints.end();
}

class TemporaryProject {
  public:
    TemporaryProject() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("codesplit_callable_inventory_" + std::to_string(suffix));
        std::filesystem::create_directories(build_path());

        std::ofstream header{header_path()};
        header << "namespace sample {\n"
                  "class Worker {\n"
                  "public:\n"
                  "    int run(int) const &;\n"
                  "    int run(double);\n"
                  "    int inline_method() { return 1; }\n"
                  "};\n"
                  "}\n";

        std::ofstream source{source_path()};
        source << "#include \"worker.hpp\"\n"
                  "namespace sample {\n"
                  "int declaration(int);\n"
                  "int helper(int value) { return value * 2; }\n"
                  "int unavailable(int) = delete;\n"
                  "int Worker::run(int value) const & { return helper(value); }\n"
                  "int Worker::run(double value) { return static_cast<int>(value); }\n"
                  "}\n"
                  "#define DEFINE_FUNCTION(name) int name() { return 1; }\n"
                  "DEFINE_FUNCTION(generated)\n"
                  "#warning CodeSplit diagnostic test\n";

        std::ofstream database{build_path() / "compile_commands.json"};
        database << "[\n"
                    "  {\n"
                    "    \"directory\": \""
                 << path_to_utf8(root_) << "\",\n"
                 << "    \"file\": \"" << path_to_utf8(source_path()) << "\",\n"
                 << "    \"arguments\": [\"clang-cl\", \"/std:c++20\", \"/c\", \""
                 << path_to_utf8(source_path()) << "\"]\n"
                 << "  }\n"
                    "]\n";
    }

    ~TemporaryProject() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    TemporaryProject(const TemporaryProject&) = delete;
    TemporaryProject& operator=(const TemporaryProject&) = delete;

    [[nodiscard]] std::filesystem::path source_path() const { return root_ / "sample.cpp"; }
    [[nodiscard]] std::filesystem::path header_path() const { return root_ / "worker.hpp"; }
    [[nodiscard]] std::filesystem::path build_path() const { return root_ / "build"; }

    void replace_source(const std::string& content) const {
        std::ofstream source{source_path()};
        source << content;
    }

  private:
    std::filesystem::path root_;
};

const codesplit::analysis::CallableDefinition*
find_callable(const codesplit::analysis::CallableInventoryResult& result, const std::string& name) {
    const auto callable = std::ranges::find(
        result.callables, name, &codesplit::analysis::CallableDefinition::qualified_name);
    return callable == result.callables.end() ? nullptr : &*callable;
}

const codesplit::analysis::CallableDefinition*
find_callable_at_line(const codesplit::analysis::CallableInventoryResult& result,
                      const std::string& name, std::uintmax_t begin_line) {
    const auto callable = std::ranges::find_if(result.callables, [&](const auto& candidate) {
        return candidate.qualified_name == name && candidate.begin_line == begin_line;
    });
    return callable == result.callables.end() ? nullptr : &*callable;
}

const codesplit::analysis::FrontendDiagnostic*
find_diagnostic(const codesplit::analysis::CallableInventoryResult& result,
                codesplit::analysis::FrontendDiagnosticSeverity severity,
                const std::string& message_fragment) {
    const auto diagnostic = std::ranges::find_if(result.diagnostics, [&](const auto& candidate) {
        return candidate.severity == severity &&
               candidate.message.find(message_fragment) != std::string::npos;
    });
    return diagnostic == result.diagnostics.end() ? nullptr : &*diagnostic;
}

void inventories_source_definitions() {
    const TemporaryProject project;
    constexpr std::uintmax_t size_limit_bytes = 32;

    const auto result = codesplit::analysis::inventory_callables(
        project.build_path(), project.source_path(), size_limit_bytes);

    expect(static_cast<bool>(result), "valid translation unit should be inventoried");
    expect(static_cast<bool>(result.compilation), "used compilation command should be returned");
    const auto* warning =
        find_diagnostic(result, codesplit::analysis::FrontendDiagnosticSeverity::warning,
                        "CodeSplit diagnostic test");
    expect(warning != nullptr, "frontend warning should be retained");
    expect(find_diagnostic(result, codesplit::analysis::FrontendDiagnosticSeverity::warning,
                           "argument unused during compilation: '/c'") == nullptr,
           "tooling-only compile flag should not produce a frontend warning");
    if (warning != nullptr) {
        expect(warning->path.filename() == project.source_path().filename(),
               "frontend warning should retain its source path");
        expect(warning->line == 11, "frontend warning should retain its source line");
    }
    expect(find_callable(result, "sample::declaration") == nullptr,
           "declarations without a body should be excluded");
    expect(find_callable(result, "sample::unavailable") == nullptr,
           "deleted definitions without a body should be excluded");
    expect(find_callable(result, "sample::Worker::inline_method") == nullptr,
           "methods defined inside a class should be excluded from this slice");

    const auto* function = find_callable(result, "sample::helper");
    expect(function != nullptr, "free function definition should be found");
    if (function != nullptr) {
        expect(function->kind == codesplit::analysis::CallableKind::free_function,
               "free function should retain its kind");
        expect(function->begin_line == 4 && function->end_line == 4,
               "free function should retain exact source lines");
        expect(function->end_offset > function->begin_offset,
               "free function should retain a non-empty source range");
        expect(function->size_bytes == function->end_offset - function->begin_offset,
               "callable size should be derived from byte offsets");
        expect(
            has_constraint(*function, codesplit::analysis::CallableConstraint::exceeds_size_limit),
            "callable larger than the configured limit should be marked");
    }

    const auto* method = find_callable_at_line(result, "sample::Worker::run", 6);
    expect(method != nullptr, "out-of-line method definition should be found");
    if (method != nullptr) {
        expect(method->kind == codesplit::analysis::CallableKind::method,
               "out-of-line method should retain its kind");
        expect(!method->symbol_id.empty(), "method should retain a stable symbol identifier");
        expect(method->declaration.has_value(), "method should link to its class declaration");
        if (method->declaration.has_value()) {
            expect(method->declaration->path.filename() == project.header_path().filename(),
                   "method declaration should retain its header path");
            expect(method->declaration->begin_line == 4,
                   "overload should link to the matching declaration");
        }
        expect(method->owning_record.has_value(), "method should link to its owning class");
        if (method->owning_record.has_value()) {
            expect(method->owning_record->path.filename() == project.header_path().filename(),
                   "owning class should retain its header path");
            expect(method->owning_record->begin_line == 2,
                   "owning class should retain its source line");
        }
    }

    const auto* overload = find_callable_at_line(result, "sample::Worker::run", 7);
    expect(overload != nullptr, "overloaded out-of-line method should be found");
    if (method != nullptr && overload != nullptr) {
        expect(!overload->symbol_id.empty(), "overload should retain a stable symbol identifier");
        expect(method->symbol_id != overload->symbol_id,
               "overloads should retain distinct symbol identifiers");
        expect(overload->declaration.has_value(), "overload should link to its declaration");
        if (overload->declaration.has_value()) {
            expect(overload->declaration->begin_line == 5,
                   "overload should not link by qualified name alone");
        }
    }

    const auto* generated = find_callable(result, "generated");
    expect(generated != nullptr, "macro-generated definition should be found");
    if (generated != nullptr) {
        expect(has_constraint(*generated, codesplit::analysis::CallableConstraint::macro_expansion),
               "macro-generated definition should retain its origin");
    }
}

void reports_frontend_errors() {
    const TemporaryProject project;
    project.replace_source("int broken( {\n");

    const auto result =
        codesplit::analysis::inventory_callables(project.build_path(), project.source_path(), 1024);

    expect(!result, "frontend errors should fail callable inventory");
    expect(result.callables.empty(), "partial callable inventory should be discarded");
    const auto* error =
        find_diagnostic(result, codesplit::analysis::FrontendDiagnosticSeverity::error, "expected");
    expect(error != nullptr, "frontend error should be retained");
    if (error != nullptr) {
        expect(error->path.filename() == project.source_path().filename(),
               "frontend error should retain its source path");
        expect(error->line == 1, "frontend error should retain its source line");
        expect(error->column > 0, "frontend error should retain its source column");
    }
}

void rejects_missing_compilation_database() {
    const TemporaryProject project;
    std::filesystem::remove(project.build_path() / "compile_commands.json");

    const auto result =
        codesplit::analysis::inventory_callables(project.build_path(), project.source_path(), 1024);

    expect(!result, "inventory should fail without a compilation database");
    expect(!result.compilation, "compilation failure should be available to the caller");
    expect(result.error.find("Unable to load compilation database") != std::string::npos,
           "inventory error should explain the missing database");
}

} // namespace

int main() {
    inventories_source_definitions();
    reports_frontend_errors();
    rejects_missing_compilation_database();

    if (failure_count == 0) {
        std::cout << "All callable-inventory tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

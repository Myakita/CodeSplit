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

        std::ofstream source{source_path()};
        source << "namespace sample {\n"
                  "int declaration(int);\n"
                  "int helper(int value) { return value * 2; }\n"
                  "class Worker {\n"
                  "public:\n"
                  "    int run(int);\n"
                  "    int inline_method() { return 1; }\n"
                  "};\n"
                  "int Worker::run(int value) { return helper(value); }\n"
                  "}\n"
                  "#define DEFINE_FUNCTION(name) int name() { return 1; }\n"
                  "DEFINE_FUNCTION(generated)\n";

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
    [[nodiscard]] std::filesystem::path build_path() const { return root_ / "build"; }

  private:
    std::filesystem::path root_;
};

const codesplit::analysis::CallableDefinition*
find_callable(const codesplit::analysis::CallableInventoryResult& result, const std::string& name) {
    const auto callable = std::ranges::find(
        result.callables, name, &codesplit::analysis::CallableDefinition::qualified_name);
    return callable == result.callables.end() ? nullptr : &*callable;
}

void inventories_source_definitions() {
    const TemporaryProject project;
    constexpr std::uintmax_t size_limit_bytes = 32;

    const auto result = codesplit::analysis::inventory_callables(
        project.build_path(), project.source_path(), size_limit_bytes);

    expect(static_cast<bool>(result), "valid translation unit should be inventoried");
    expect(static_cast<bool>(result.compilation), "used compilation command should be returned");
    expect(find_callable(result, "sample::declaration") == nullptr,
           "declarations without a body should be excluded");
    expect(find_callable(result, "sample::Worker::inline_method") == nullptr,
           "methods defined inside a class should be excluded from this slice");

    const auto* function = find_callable(result, "sample::helper");
    expect(function != nullptr, "free function definition should be found");
    if (function != nullptr) {
        expect(function->kind == codesplit::analysis::CallableKind::free_function,
               "free function should retain its kind");
        expect(function->begin_line == 3 && function->end_line == 3,
               "free function should retain exact source lines");
        expect(function->end_offset > function->begin_offset,
               "free function should retain a non-empty source range");
        expect(function->size_bytes == function->end_offset - function->begin_offset,
               "callable size should be derived from byte offsets");
        expect(
            has_constraint(*function, codesplit::analysis::CallableConstraint::exceeds_size_limit),
            "callable larger than the configured limit should be marked");
    }

    const auto* method = find_callable(result, "sample::Worker::run");
    expect(method != nullptr, "out-of-line method definition should be found");
    if (method != nullptr) {
        expect(method->kind == codesplit::analysis::CallableKind::method,
               "out-of-line method should retain its kind");
    }

    const auto* generated = find_callable(result, "generated");
    expect(generated != nullptr, "macro-generated definition should be found");
    if (generated != nullptr) {
        expect(has_constraint(*generated, codesplit::analysis::CallableConstraint::macro_expansion),
               "macro-generated definition should retain its origin");
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
    rejects_missing_compilation_database();

    if (failure_count == 0) {
        std::cout << "All callable-inventory tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

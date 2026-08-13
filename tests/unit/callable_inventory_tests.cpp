#include "codesplit/analysis/callable_inventory.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

        std::ofstream nested_header{nested_header_path()};
        nested_header << "#pragma once\n";

        std::ofstream header{header_path()};
        header << "#include \"nested.hpp\"\n"
                  "namespace sample {\n"
                  "class Worker {\n"
                  "public:\n"
                  "    int run(int) const &;\n"
                  "    int run(double);\n"
                  "    int inline_method() { return 1; }\n"
                  "};\n"
                  "struct Payload { int value; }; int consume(const Payload&);\n"
                  "}\n";

        std::ofstream source{source_path()};
        source << "#include \"worker.hpp\"\n"
                  "namespace sample {\n"
                  "int declaration(int);\n"
                  "int helper(int value) { return value * 2; }\n"
                  "int unavailable(int) = delete;\n"
                  "int Worker::run(int value) const & { return helper(value); }\n"
                  "int Worker::run(double value) { return helper(static_cast<int>(value)); }\n"
                  "int consume(const Payload& payload) { Payload copy = payload; return "
                  "copy.value; }\n"
                  "int counter = 0;\n"
                  "int read_counter() { return counter; }\n"
                  "void set_counter(int value) { (counter) = value; }\n"
                  "void increment_counter() { ++(counter); }\n"
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
                 << "    \"arguments\": [\"clang-cl\", \"/std:c++20\", \"/I" << path_to_utf8(root_)
                 << "\", \"/c\", \"" << path_to_utf8(source_path()) << "\"]\n"
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
    [[nodiscard]] std::filesystem::path nested_header_path() const { return root_ / "nested.hpp"; }
    [[nodiscard]] std::filesystem::path build_path() const { return root_ / "build"; }

    void replace_source(const std::string& content) const {
        std::ofstream source{source_path()};
        source << content;
    }

  private:
    std::filesystem::path root_;
};

class LanguageProject {
  public:
    LanguageProject(const std::string& extension, const std::string& compiler,
                    const std::string& standard, const std::string& content)
        : extension_{extension} {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("codesplit_language_matrix_" + extension + '_' + std::to_string(suffix));
        std::filesystem::create_directories(build_path());

        std::ofstream source{source_path()};
        source << content;

        std::ofstream database{build_path() / "compile_commands.json"};
        database << "[\n"
                    "  {\n"
                    "    \"directory\": \""
                 << path_to_utf8(root_) << "\",\n"
                 << "    \"file\": \"" << path_to_utf8(source_path()) << "\",\n"
                 << "    \"arguments\": [\"" << compiler << "\", \"" << standard << "\", \"-c\", \""
                 << path_to_utf8(source_path()) << "\"]\n"
                 << "  }\n"
                    "]\n";
    }

    ~LanguageProject() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    LanguageProject(const LanguageProject&) = delete;
    LanguageProject& operator=(const LanguageProject&) = delete;

    [[nodiscard]] std::filesystem::path source_path() const {
        return root_ / ("sample." + extension_);
    }
    [[nodiscard]] std::filesystem::path build_path() const { return root_ / "build"; }

  private:
    std::filesystem::path root_;
    std::string extension_;
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

const codesplit::analysis::CallableDependency*
find_dependency(const codesplit::analysis::CallableInventoryResult& result,
                const std::string& source_symbol_id, const std::string& target_symbol_id) {
    const auto dependency = std::ranges::find_if(result.dependencies, [&](const auto& candidate) {
        return candidate.source_symbol_id == source_symbol_id &&
               candidate.target_symbol_id == target_symbol_id;
    });
    return dependency == result.dependencies.end() ? nullptr : &*dependency;
}

const codesplit::analysis::CallableDependency*
find_dependency_to(const codesplit::analysis::CallableInventoryResult& result,
                   const std::string& source_symbol_id, const std::string& target_qualified_name) {
    const auto dependency = std::ranges::find_if(result.dependencies, [&](const auto& candidate) {
        return candidate.source_symbol_id == source_symbol_id &&
               candidate.target_qualified_name == target_qualified_name;
    });
    return dependency == result.dependencies.end() ? nullptr : &*dependency;
}

const codesplit::analysis::MacroDependency*
find_macro_dependency(const codesplit::analysis::CallableInventoryResult& result,
                      const std::string& source_symbol_id, const std::string& macro_name) {
    const auto dependency = std::ranges::find_if(result.macros, [&](const auto& candidate) {
        return candidate.source_symbol_id == source_symbol_id && candidate.macro_name == macro_name;
    });
    return dependency == result.macros.end() ? nullptr : &*dependency;
}

const codesplit::analysis::CallableDependency*
find_dependency_to(const codesplit::analysis::CallableInventoryResult& result,
                   const std::string& source_symbol_id, const std::string& target_qualified_name,
                   codesplit::analysis::CallableDependencyKind kind) {
    const auto dependency = std::ranges::find_if(result.dependencies, [&](const auto& candidate) {
        return candidate.kind == kind && candidate.source_symbol_id == source_symbol_id &&
               candidate.target_qualified_name == target_qualified_name;
    });
    return dependency == result.dependencies.end() ? nullptr : &*dependency;
}

void inventories_source_definitions() {
    const TemporaryProject project;
    constexpr std::uintmax_t size_limit_bytes = 8;

    const auto result = codesplit::analysis::inventory_callables(
        project.build_path(), project.source_path(), size_limit_bytes);

    expect(static_cast<bool>(result), "valid translation unit should be inventoried");
    expect(static_cast<bool>(result.compilation), "used compilation command should be returned");
    expect(result.includes.size() == 1,
           "only direct includes from the main file should be retained");
    if (result.includes.size() == 1) {
        const auto& include = result.includes.front();
        expect(include.kind == codesplit::analysis::IncludeKind::quoted,
               "quoted include should retain its kind");
        expect(include.written_name == "worker.hpp",
               "include should retain the name written in source");
        expect(include.resolved_path.filename() == project.header_path().filename(),
               "include should retain its resolved target path");
        expect(include.origin.path.filename() == project.source_path().filename(),
               "include should retain the file containing the directive");
        expect(include.origin.begin_line == 1 && include.origin.end_line == 1,
               "include should retain its source line");
        expect(include.origin.end_offset > include.origin.begin_offset,
               "include should retain a non-empty source range");
    }
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
        expect(warning->line == 16, "frontend warning should retain its source line");
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
            expect(method->declaration->begin_line == 5,
                   "overload should link to the matching declaration");
        }
        expect(method->owning_record.has_value(), "method should link to its owning class");
        if (method->owning_record.has_value()) {
            expect(method->owning_record->path.filename() == project.header_path().filename(),
                   "owning class should retain its header path");
            expect(method->owning_record->begin_line == 3,
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
            expect(overload->declaration->begin_line == 6,
                   "overload should not link by qualified name alone");
        }
    }

    if (function != nullptr && method != nullptr && overload != nullptr) {
        const auto* method_call = find_dependency(result, method->symbol_id, function->symbol_id);
        const auto* overload_call =
            find_dependency(result, overload->symbol_id, function->symbol_id);
        expect(method_call != nullptr, "method call should produce a dependency edge");
        expect(overload_call != nullptr, "overloaded method call should produce a dependency edge");
        if (method_call != nullptr) {
            expect(method_call->kind == codesplit::analysis::CallableDependencyKind::direct_call,
                   "direct call should retain its dependency kind");
            expect(method_call->source_qualified_name == "sample::Worker::run",
                   "dependency should retain the source name");
            expect(method_call->target_qualified_name == "sample::helper",
                   "dependency should retain the target name");
        }
    }

    const auto* consumer = find_callable(result, "sample::consume");
    expect(consumer != nullptr, "function using a record type should be found");
    if (consumer != nullptr) {
        const auto* type_reference =
            find_dependency_to(result, consumer->symbol_id, "sample::Payload");
        expect(type_reference != nullptr, "record use should produce a type dependency edge");
        if (type_reference != nullptr) {
            expect(type_reference->kind ==
                       codesplit::analysis::CallableDependencyKind::type_reference,
                   "record use should retain the type dependency kind");
            expect(!type_reference->target_symbol_id.empty(),
                   "type dependency should retain the record USR");
            const auto matching_references =
                std::ranges::count_if(result.dependencies, [&](const auto& dependency) {
                    return dependency.kind ==
                               codesplit::analysis::CallableDependencyKind::type_reference &&
                           dependency.source_symbol_id == consumer->symbol_id &&
                           dependency.target_symbol_id == type_reference->target_symbol_id;
                });
            expect(matching_references == 1,
                   "repeated uses of one record should produce one dependency edge");
        }
    }

    const auto* reader = find_callable(result, "sample::read_counter");
    const auto* writer = find_callable(result, "sample::set_counter");
    const auto* increment = find_callable(result, "sample::increment_counter");
    expect(reader != nullptr && writer != nullptr && increment != nullptr,
           "global access functions should be found");
    if (reader != nullptr && writer != nullptr && increment != nullptr) {
        expect(find_dependency_to(result, reader->symbol_id, "sample::counter",
                                  codesplit::analysis::CallableDependencyKind::global_read) !=
                   nullptr,
               "reading a global should produce a read dependency");
        expect(find_dependency_to(result, reader->symbol_id, "sample::counter",
                                  codesplit::analysis::CallableDependencyKind::global_write) ==
                   nullptr,
               "reading a global should not produce a write dependency");
        expect(find_dependency_to(result, writer->symbol_id, "sample::counter",
                                  codesplit::analysis::CallableDependencyKind::global_write) !=
                   nullptr,
               "assigning a global should produce a write dependency");
        expect(find_dependency_to(result, writer->symbol_id, "sample::counter",
                                  codesplit::analysis::CallableDependencyKind::global_read) ==
                   nullptr,
               "simple assignment should not produce a read dependency");
        expect(find_dependency_to(result, increment->symbol_id, "sample::counter",
                                  codesplit::analysis::CallableDependencyKind::global_read) !=
                   nullptr,
               "incrementing a global should produce a read dependency");
        expect(find_dependency_to(result, increment->symbol_id, "sample::counter",
                                  codesplit::analysis::CallableDependencyKind::global_write) !=
                   nullptr,
               "incrementing a global should produce a write dependency");
    }

    const auto* generated = find_callable(result, "generated");
    expect(generated != nullptr, "macro-generated definition should be found");
    if (generated != nullptr) {
        expect(has_constraint(*generated, codesplit::analysis::CallableConstraint::macro_expansion),
               "macro-generated definition should retain its origin");
        expect(
            has_constraint(*generated, codesplit::analysis::CallableConstraint::exceeds_size_limit),
            "macro-generated oversized definition should retain both constraints");
        const auto* macro = find_macro_dependency(result, generated->symbol_id, "DEFINE_FUNCTION");
        expect(macro != nullptr, "macro generating the callable should produce a macro dependency");
        if (macro != nullptr) {
            expect(macro->definition.has_value() && macro->definition->begin_line == 14,
                   "generating macro should retain its definition line");
            expect(macro->expansions.size() == 1 && macro->expansions.front().begin_line == 15,
                   "generating macro should retain its invocation line");
        }
    }
}

void reports_frontend_errors() {
    const TemporaryProject project;
    project.replace_source("#include \"worker.hpp\"\nint broken( {\n");

    const auto result =
        codesplit::analysis::inventory_callables(project.build_path(), project.source_path(), 1024);

    expect(!result, "frontend errors should fail callable inventory");
    expect(result.callables.empty(), "partial callable inventory should be discarded");
    expect(result.dependencies.empty(), "partial dependencies should be discarded");
    expect(result.includes.empty(), "partial includes should be discarded");
    expect(result.macros.empty(), "partial macro dependencies should be discarded");
    const auto* error =
        find_diagnostic(result, codesplit::analysis::FrontendDiagnosticSeverity::error, "expected");
    expect(error != nullptr, "frontend error should be retained");
    if (error != nullptr) {
        expect(error->path.filename() == project.source_path().filename(),
               "frontend error should retain its source path");
        expect(error->line == 2, "frontend error should retain its source line");
        expect(error->column > 0, "frontend error should retain its source column");
    }
}

void inventories_angled_include() {
    const TemporaryProject project;
    project.replace_source("#include <worker.hpp>\n");

    const auto result =
        codesplit::analysis::inventory_callables(project.build_path(), project.source_path(), 1024);

    expect(static_cast<bool>(result), "source with an angled include should be inventoried");
    expect(result.includes.size() == 1, "angled include should produce one direct dependency");
    if (result.includes.size() == 1) {
        expect(result.includes.front().kind == codesplit::analysis::IncludeKind::angled,
               "angled include should retain its kind");
        expect(result.includes.front().written_name == "worker.hpp",
               "angled include should retain its written name");
    }
}

void inventories_callable_linkage() {
    const TemporaryProject project;
    project.replace_source("int external_function() { return 1; }\n"
                           "static int internal_function() { return 2; }\n"
                           "namespace { int anonymous_function() { return 3; } }\n");

    const auto result =
        codesplit::analysis::inventory_callables(project.build_path(), project.source_path(), 1024);

    expect(static_cast<bool>(result), "callable linkage source should be inventoried");
    const auto* external = find_callable(result, "external_function");
    const auto* internal = find_callable(result, "internal_function");
    const auto* anonymous = find_callable(result, "(anonymous namespace)::anonymous_function");
    expect(external != nullptr && internal != nullptr && anonymous != nullptr,
           "external, static, and anonymous callables should be found");
    if (external != nullptr) {
        expect(external->linkage == codesplit::analysis::SymbolLinkage::external,
               "ordinary namespace function should have external linkage");
        expect(!external->in_anonymous_namespace,
               "ordinary namespace function should not be marked anonymous");
    }
    if (internal != nullptr) {
        expect(internal->linkage == codesplit::analysis::SymbolLinkage::internal,
               "static namespace function should have internal linkage");
        expect(!internal->in_anonymous_namespace,
               "static namespace function should not be marked anonymous");
    }
    if (anonymous != nullptr) {
        expect(anonymous->linkage == codesplit::analysis::SymbolLinkage::internal,
               "anonymous namespace function should retain internal linkage");
        expect(anonymous->in_anonymous_namespace,
               "anonymous namespace function should retain its semantic origin");
    }
}

void inventories_direct_macro_dependencies() {
    const TemporaryProject project;
    project.replace_source("#define OFFSET 1\n"
                           "#define ADD_OFFSET(value) ((value) + OFFSET)\n"
                           "int unrelated = ADD_OFFSET(1);\n"
                           "int calculate(int value) {\n"
                           "    return ADD_OFFSET(value) + ADD_OFFSET(value);\n"
                           "}\n");

    const auto result =
        codesplit::analysis::inventory_callables(project.build_path(), project.source_path(), 1024);

    expect(static_cast<bool>(result), "source using macros should be inventoried");
    const auto* callable = find_callable(result, "calculate");
    expect(callable != nullptr, "callable using macros should be found");
    if (callable == nullptr) {
        return;
    }

    expect(result.macros.size() == 1,
           "only direct macro use inside the callable should produce a dependency");
    const auto* dependency = find_macro_dependency(result, callable->symbol_id, "ADD_OFFSET");
    expect(dependency != nullptr, "direct macro use should produce a dependency");
    if (dependency != nullptr) {
        expect(dependency->source_qualified_name == "calculate",
               "macro dependency should retain the source name");
        expect(dependency->definition.has_value(),
               "source-defined macro should retain its definition range");
        if (dependency->definition.has_value()) {
            expect(dependency->definition->begin_line == 2,
                   "macro dependency should retain the definition line");
        }
        expect(dependency->expansions.size() == 2,
               "repeated uses should share one edge with two origins");
        if (dependency->expansions.size() == 2) {
            expect(dependency->expansions[0].begin_line == 5 &&
                       dependency->expansions[1].begin_line == 5,
                   "macro uses should retain their expansion line");
            expect(dependency->expansions[0].begin_offset != dependency->expansions[1].begin_offset,
                   "separate macro uses should retain distinct offsets");
        }
    }
    expect(find_macro_dependency(result, callable->symbol_id, "OFFSET") == nullptr,
           "nested macro expansion should not be reported as direct source use");
}

void inventories_c_and_cpp_language_standards() {
    struct LanguageCase {
        std::string extension;
        std::string compiler;
        std::string standard;
        std::string content;
        std::string callable_name;
    };

    const std::vector<LanguageCase> cases{
        {
            .extension = "c",
            .compiler = "clang",
            .standard = "-std=c11",
            .content = "_Static_assert(_Generic(1, int: 1, default: 0), \"C11 required\");\n"
                       "int calculate(int value) { return value + 1; }\n",
            .callable_name = "calculate",
        },
        {
            .extension = "c",
            .compiler = "clang",
            .standard = "-std=c17",
            .content = "_Static_assert(sizeof(int) >= 2, \"C17 translation unit\");\n"
                       "int calculate(int value) { return value + 1; }\n",
            .callable_name = "calculate",
        },
        {
            .extension = "cpp",
            .compiler = "clang++",
            .standard = "-std=c++17",
            .content = "namespace sample {\n"
                       "constexpr int language_version() {\n"
                       "    if constexpr (true) { return 17; }\n"
                       "}\n"
                       "int calculate(int value) { return value + 1; }\n"
                       "}\n",
            .callable_name = "sample::calculate",
        },
        {
            .extension = "cpp",
            .compiler = "clang++",
            .standard = "-std=c++20",
            .content = "namespace sample {\n"
                       "template <typename T> concept Number = true;\n"
                       "static_assert(Number<int>);\n"
                       "int calculate(int value) { return value + 1; }\n"
                       "}\n",
            .callable_name = "sample::calculate",
        },
    };

    for (const auto& language : cases) {
        const LanguageProject project{language.extension, language.compiler, language.standard,
                                      language.content};
        const auto result = codesplit::analysis::inventory_callables(project.build_path(),
                                                                     project.source_path(), 1024);

        expect(static_cast<bool>(result), language.standard + " source should be inventoried");
        expect(result.diagnostics.empty(),
               language.standard + " source should not produce diagnostics");
        expect(find_callable(result, language.callable_name) != nullptr,
               language.standard + " callable should be found");
        expect(std::ranges::find(result.compilation.command.arguments, language.standard) !=
                   result.compilation.command.arguments.end(),
               language.standard + " flag should be preserved");
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
    inventories_angled_include();
    inventories_callable_linkage();
    inventories_direct_macro_dependencies();
    inventories_c_and_cpp_language_standards();
    rejects_missing_compilation_database();

    if (failure_count == 0) {
        std::cout << "All callable-inventory tests passed.\n";
    }

    return failure_count == 0 ? 0 : 1;
}

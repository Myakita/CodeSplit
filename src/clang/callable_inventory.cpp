#include "codesplit/analysis/callable_inventory.hpp"
#include "compilation_database_internal.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/TypeLoc.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace codesplit::analysis {
namespace {

void add_constraint(CallableDefinition& callable, CallableConstraint constraint) {
    callable.constraints.push_back(constraint);
}

std::optional<FrontendDiagnosticSeverity>
diagnostic_severity(clang::DiagnosticsEngine::Level level) {
    switch (level) {
    case clang::DiagnosticsEngine::Ignored:
        return std::nullopt;
    case clang::DiagnosticsEngine::Note:
        return FrontendDiagnosticSeverity::note;
    case clang::DiagnosticsEngine::Remark:
        return FrontendDiagnosticSeverity::remark;
    case clang::DiagnosticsEngine::Warning:
        return FrontendDiagnosticSeverity::warning;
    case clang::DiagnosticsEngine::Error:
        return FrontendDiagnosticSeverity::error;
    case clang::DiagnosticsEngine::Fatal:
        return FrontendDiagnosticSeverity::fatal;
    }
    return std::nullopt;
}

class CollectingDiagnosticConsumer : public clang::DiagnosticConsumer {
  public:
    explicit CollectingDiagnosticConsumer(std::vector<FrontendDiagnostic>& diagnostics)
        : diagnostics_{diagnostics} {}

    void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                          const clang::Diagnostic& information) override {
        clang::DiagnosticConsumer::HandleDiagnostic(level, information);
        const auto severity = diagnostic_severity(level);
        if (!severity.has_value()) {
            return;
        }

        llvm::SmallString<256> message;
        information.FormatDiagnostic(message);
        FrontendDiagnostic diagnostic{
            .severity = *severity,
            .message = std::string{message.begin(), message.end()},
            .path = {},
            .line = 0,
            .column = 0,
        };

        const auto location = information.getLocation();
        if (location.isValid()) {
            auto& source_manager = information.getSourceManager();
            const auto expansion_location = source_manager.getExpansionLoc(location);
            const auto filename = source_manager.getFilename(expansion_location);
            if (!filename.empty()) {
                diagnostic.path = std::filesystem::path{filename.str()};
            }
            diagnostic.line = source_manager.getExpansionLineNumber(location);
            diagnostic.column = source_manager.getExpansionColumnNumber(location);
        }

        diagnostics_.push_back(std::move(diagnostic));
    }

  private:
    std::vector<FrontendDiagnostic>& diagnostics_;
};

std::string symbol_id_for(const clang::Decl& declaration) {
    llvm::SmallString<128> symbol_id;
    if (clang::index::generateUSRForDecl(&declaration, symbol_id)) {
        return {};
    }
    return std::string{symbol_id};
}

std::optional<SourceRange> source_range_for(const clang::SourceRange source_range,
                                            clang::SourceManager& source_manager,
                                            const clang::LangOptions& language_options) {
    const auto begin = source_manager.getExpansionLoc(source_range.getBegin());
    const auto token_end = source_manager.getExpansionLoc(source_range.getEnd());
    if (begin.isInvalid() || token_end.isInvalid()) {
        return std::nullopt;
    }

    const auto end =
        clang::Lexer::getLocForEndOfToken(token_end, 0, source_manager, language_options);
    if (end.isInvalid() || source_manager.getFileID(begin) != source_manager.getFileID(end)) {
        return std::nullopt;
    }

    const auto filename = source_manager.getFilename(begin);
    if (filename.empty()) {
        return std::nullopt;
    }

    return SourceRange{
        .path = std::filesystem::path{filename.str()},
        .begin_offset = source_manager.getFileOffset(begin),
        .end_offset = source_manager.getFileOffset(end),
        .begin_line = source_manager.getExpansionLineNumber(source_range.getBegin()),
        .end_line = source_manager.getExpansionLineNumber(source_range.getEnd()),
    };
}

std::optional<SourceRange> include_origin_for(clang::SourceLocation hash_location,
                                              clang::CharSourceRange filename_range,
                                              clang::SourceManager& source_manager,
                                              const clang::LangOptions& language_options) {
    const auto begin = source_manager.getExpansionLoc(hash_location);
    auto end = source_manager.getExpansionLoc(filename_range.getEnd());
    if (filename_range.isTokenRange()) {
        end = clang::Lexer::getLocForEndOfToken(end, 0, source_manager, language_options);
    }
    if (begin.isInvalid() || end.isInvalid() ||
        source_manager.getFileID(begin) != source_manager.getFileID(end)) {
        return std::nullopt;
    }

    const auto filename = source_manager.getFilename(begin);
    if (filename.empty()) {
        return std::nullopt;
    }

    return SourceRange{
        .path = std::filesystem::path{filename.str()},
        .begin_offset = source_manager.getFileOffset(begin),
        .end_offset = source_manager.getFileOffset(end),
        .begin_line = source_manager.getExpansionLineNumber(begin),
        .end_line = source_manager.getExpansionLineNumber(end),
    };
}

class DirectIncludeCollector : public clang::PPCallbacks {
  public:
    DirectIncludeCollector(clang::SourceManager& source_manager,
                           const clang::LangOptions& language_options,
                           std::vector<IncludeDependency>& includes)
        : source_manager_{source_manager}, language_options_{language_options},
          includes_{includes} {}

    void InclusionDirective(clang::SourceLocation hash_location, const clang::Token&,
                            llvm::StringRef filename, bool is_angled,
                            clang::CharSourceRange filename_range, clang::OptionalFileEntryRef file,
                            llvm::StringRef, llvm::StringRef, const clang::Module*, bool,
                            clang::SrcMgr::CharacteristicKind) override {
        const auto begin = source_manager_.getExpansionLoc(hash_location);
        if (!begin.isValid() || !source_manager_.isWrittenInMainFile(begin)) {
            return;
        }

        const auto origin =
            include_origin_for(hash_location, filename_range, source_manager_, language_options_);
        if (!origin.has_value()) {
            return;
        }

        includes_.push_back({
            .kind = is_angled ? IncludeKind::angled : IncludeKind::quoted,
            .written_name = filename.str(),
            .resolved_path = file.has_value() ? std::filesystem::path{file->getName().str()}
                                              : std::filesystem::path{},
            .origin = *origin,
        });
    }

  private:
    clang::SourceManager& source_manager_;
    const clang::LangOptions& language_options_;
    std::vector<IncludeDependency>& includes_;
};

void add_dependency(std::vector<CallableDependency>& dependencies, CallableDependencyKind kind,
                    const std::string& source_symbol_id, const std::string& source_qualified_name,
                    const clang::NamedDecl& canonical_target) {
    if (source_symbol_id.empty()) {
        return;
    }

    const auto target_symbol_id = symbol_id_for(canonical_target);
    if (target_symbol_id.empty()) {
        return;
    }

    const auto duplicate = std::ranges::any_of(dependencies, [&](const auto& dependency) {
        return dependency.kind == kind && dependency.source_symbol_id == source_symbol_id &&
               dependency.target_symbol_id == target_symbol_id;
    });
    if (!duplicate) {
        dependencies.push_back({
            .kind = kind,
            .source_symbol_id = source_symbol_id,
            .source_qualified_name = source_qualified_name,
            .target_symbol_id = target_symbol_id,
            .target_qualified_name = canonical_target.getQualifiedNameAsString(),
        });
    }
}

class DirectCallVisitor : public clang::RecursiveASTVisitor<DirectCallVisitor> {
  public:
    DirectCallVisitor(std::string source_symbol_id, std::string source_qualified_name,
                      std::vector<CallableDependency>& dependencies)
        : source_symbol_id_{std::move(source_symbol_id)},
          source_qualified_name_{std::move(source_qualified_name)}, dependencies_{dependencies} {}

    bool VisitCallExpr(clang::CallExpr* expression) {
        const auto* target = expression->getDirectCallee();
        if (target == nullptr) {
            return true;
        }

        const auto* canonical_target = target->getCanonicalDecl();
        add_dependency(dependencies_, CallableDependencyKind::direct_call, source_symbol_id_,
                       source_qualified_name_, *canonical_target);
        return true;
    }

  private:
    std::string source_symbol_id_;
    std::string source_qualified_name_;
    std::vector<CallableDependency>& dependencies_;
};

class TypeReferenceVisitor : public clang::RecursiveASTVisitor<TypeReferenceVisitor> {
  public:
    TypeReferenceVisitor(std::string source_symbol_id, std::string source_qualified_name,
                         std::vector<CallableDependency>& dependencies)
        : source_symbol_id_{std::move(source_symbol_id)},
          source_qualified_name_{std::move(source_qualified_name)}, dependencies_{dependencies} {}

    bool VisitTagTypeLoc(clang::TagTypeLoc type_location) {
        const auto* target = type_location.getDecl();
        if (target != nullptr) {
            add_dependency(dependencies_, CallableDependencyKind::type_reference, source_symbol_id_,
                           source_qualified_name_, *target->getCanonicalDecl());
        }
        return true;
    }

  private:
    std::string source_symbol_id_;
    std::string source_qualified_name_;
    std::vector<CallableDependency>& dependencies_;
};

class GlobalDataVisitor : public clang::RecursiveASTVisitor<GlobalDataVisitor> {
  public:
    GlobalDataVisitor(clang::ASTContext& context, std::string source_symbol_id,
                      std::string source_qualified_name,
                      std::vector<CallableDependency>& dependencies)
        : context_{context}, source_symbol_id_{std::move(source_symbol_id)},
          source_qualified_name_{std::move(source_qualified_name)}, dependencies_{dependencies} {}

    bool VisitDeclRefExpr(clang::DeclRefExpr* expression) {
        const auto* variable = llvm::dyn_cast<clang::VarDecl>(expression->getDecl());
        if (variable == nullptr || !variable->hasGlobalStorage() || variable->isLocalVarDecl()) {
            return true;
        }

        const auto* canonical_variable = variable->getCanonicalDecl();
        auto node = clang::DynTypedNode::create(*expression);
        while (true) {
            const auto parents = context_.getParents(node);
            if (parents.empty()) {
                break;
            }

            const auto& parent = parents[0];
            if (parent.get<clang::ParenExpr>() != nullptr ||
                parent.get<clang::ImplicitCastExpr>() != nullptr) {
                node = parent;
                continue;
            }

            if (const auto* binary = parent.get<clang::BinaryOperator>();
                binary != nullptr && binary->isAssignmentOp() &&
                binary->getLHS()->IgnoreParenImpCasts() == expression) {
                add_dependency(dependencies_, CallableDependencyKind::global_write,
                               source_symbol_id_, source_qualified_name_, *canonical_variable);
                if (binary->isCompoundAssignmentOp()) {
                    add_dependency(dependencies_, CallableDependencyKind::global_read,
                                   source_symbol_id_, source_qualified_name_, *canonical_variable);
                }
                return true;
            }

            if (const auto* unary = parent.get<clang::UnaryOperator>();
                unary != nullptr && unary->isIncrementDecrementOp()) {
                add_dependency(dependencies_, CallableDependencyKind::global_read,
                               source_symbol_id_, source_qualified_name_, *canonical_variable);
                add_dependency(dependencies_, CallableDependencyKind::global_write,
                               source_symbol_id_, source_qualified_name_, *canonical_variable);
                return true;
            }
            break;
        }

        add_dependency(dependencies_, CallableDependencyKind::global_read, source_symbol_id_,
                       source_qualified_name_, *canonical_variable);
        return true;
    }

  private:
    clang::ASTContext& context_;
    std::string source_symbol_id_;
    std::string source_qualified_name_;
    std::vector<CallableDependency>& dependencies_;
};

class CallableVisitor : public clang::RecursiveASTVisitor<CallableVisitor> {
  public:
    CallableVisitor(clang::SourceManager& source_manager,
                    const clang::LangOptions& language_options, std::uintmax_t size_limit_bytes,
                    std::vector<CallableDefinition>& callables,
                    std::vector<CallableDependency>& dependencies)
        : source_manager_{source_manager}, language_options_{language_options},
          size_limit_bytes_{size_limit_bytes}, callables_{callables}, dependencies_{dependencies} {}

    bool VisitFunctionDecl(clang::FunctionDecl* declaration) {
        if (!declaration->isThisDeclarationADefinition() || declaration->isImplicit()) {
            return true;
        }

        const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration);
        if (method != nullptr && !method->isOutOfLine()) {
            return true;
        }

        const auto declaration_begin = declaration->getBeginLoc();
        const auto begin = source_manager_.getExpansionLoc(declaration_begin);
        if (!begin.isValid() || !source_manager_.isWrittenInMainFile(begin)) {
            return true;
        }

        const auto* body = declaration->getBody();
        if (body == nullptr) {
            return true;
        }

        const auto body_end = body->getEndLoc();
        const auto expanded_end = source_manager_.getExpansionLoc(body_end);
        const auto* canonical_declaration = declaration->getCanonicalDecl();
        CallableDefinition callable;
        callable.kind = method == nullptr ? CallableKind::free_function : CallableKind::method;
        callable.qualified_name = declaration->getQualifiedNameAsString();
        callable.symbol_id = symbol_id_for(*canonical_declaration);
        if (canonical_declaration != declaration) {
            callable.declaration = source_range_for(canonical_declaration->getSourceRange(),
                                                    source_manager_, language_options_);
        }
        if (const auto* canonical_method =
                llvm::dyn_cast<clang::CXXMethodDecl>(canonical_declaration)) {
            callable.owning_record =
                source_range_for(canonical_method->getParent()->getSourceRange(), source_manager_,
                                 language_options_);
        }

        DirectCallVisitor direct_call_visitor{callable.symbol_id, callable.qualified_name,
                                              dependencies_};
        direct_call_visitor.TraverseStmt(const_cast<clang::Stmt*>(body));
        TypeReferenceVisitor type_reference_visitor{callable.symbol_id, callable.qualified_name,
                                                    dependencies_};
        type_reference_visitor.TraverseDecl(declaration);
        GlobalDataVisitor global_data_visitor{declaration->getASTContext(), callable.symbol_id,
                                              callable.qualified_name, dependencies_};
        global_data_visitor.TraverseStmt(const_cast<clang::Stmt*>(body));

        if (declaration_begin.isMacroID() || body_end.isMacroID()) {
            add_constraint(callable, CallableConstraint::macro_expansion);
        }

        callable.begin_line = source_manager_.getExpansionLineNumber(declaration_begin);
        callable.end_line = source_manager_.getExpansionLineNumber(body_end);

        const auto end =
            clang::Lexer::getLocForEndOfToken(expanded_end, 0, source_manager_, language_options_);
        if (end.isInvalid() || source_manager_.getFileID(begin) != source_manager_.getFileID(end)) {
            add_constraint(callable, CallableConstraint::source_range_unavailable);
            callables_.push_back(std::move(callable));
            return true;
        }

        callable.begin_offset = source_manager_.getFileOffset(begin);
        callable.end_offset = source_manager_.getFileOffset(end);
        if (callable.end_offset < callable.begin_offset) {
            add_constraint(callable, CallableConstraint::source_range_unavailable);
            callables_.push_back(std::move(callable));
            return true;
        }

        callable.size_bytes = callable.end_offset - callable.begin_offset;

        if (callable.size_bytes > size_limit_bytes_) {
            add_constraint(callable, CallableConstraint::exceeds_size_limit);
        }

        callables_.push_back(std::move(callable));
        return true;
    }

  private:
    clang::SourceManager& source_manager_;
    const clang::LangOptions& language_options_;
    std::uintmax_t size_limit_bytes_;
    std::vector<CallableDefinition>& callables_;
    std::vector<CallableDependency>& dependencies_;
};

class CallableConsumer : public clang::ASTConsumer {
  public:
    CallableConsumer(clang::SourceManager& source_manager,
                     const clang::LangOptions& language_options, std::uintmax_t size_limit_bytes,
                     std::vector<CallableDefinition>& callables,
                     std::vector<CallableDependency>& dependencies)
        : visitor_{source_manager, language_options, size_limit_bytes, callables, dependencies} {}

    void HandleTranslationUnit(clang::ASTContext& context) override {
        visitor_.TraverseDecl(context.getTranslationUnitDecl());
    }

  private:
    CallableVisitor visitor_;
};

class CallableAction : public clang::ASTFrontendAction {
  public:
    CallableAction(std::uintmax_t size_limit_bytes, std::vector<CallableDefinition>& callables,
                   std::vector<CallableDependency>& dependencies,
                   std::vector<IncludeDependency>& includes)
        : size_limit_bytes_{size_limit_bytes}, callables_{callables}, dependencies_{dependencies},
          includes_{includes} {}

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler,
                                                          llvm::StringRef) override {
        compiler.getPreprocessor().addPPCallbacks(std::make_unique<DirectIncludeCollector>(
            compiler.getSourceManager(), compiler.getLangOpts(), includes_));
        return std::make_unique<CallableConsumer>(compiler.getSourceManager(),
                                                  compiler.getLangOpts(), size_limit_bytes_,
                                                  callables_, dependencies_);
    }

  private:
    std::uintmax_t size_limit_bytes_;
    std::vector<CallableDefinition>& callables_;
    std::vector<CallableDependency>& dependencies_;
    std::vector<IncludeDependency>& includes_;
};

class CallableActionFactory : public clang::tooling::FrontendActionFactory {
  public:
    CallableActionFactory(std::uintmax_t size_limit_bytes,
                          std::vector<CallableDefinition>& callables,
                          std::vector<CallableDependency>& dependencies,
                          std::vector<IncludeDependency>& includes)
        : size_limit_bytes_{size_limit_bytes}, callables_{callables}, dependencies_{dependencies},
          includes_{includes} {}

    std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<CallableAction>(size_limit_bytes_, callables_, dependencies_,
                                                includes_);
    }

  private:
    std::uintmax_t size_limit_bytes_;
    std::vector<CallableDefinition>& callables_;
    std::vector<CallableDependency>& dependencies_;
    std::vector<IncludeDependency>& includes_;
};

class SingleCommandDatabase : public clang::tooling::CompilationDatabase {
  public:
    explicit SingleCommandDatabase(clang::tooling::CompileCommand command)
        : command_{std::move(command)} {}

    std::vector<clang::tooling::CompileCommand> getCompileCommands(llvm::StringRef) const override {
        return {command_};
    }

    std::vector<clang::tooling::CompileCommand> getAllCompileCommands() const override {
        return {command_};
    }

  private:
    clang::tooling::CompileCommand command_;
};

CompilationCommandResult public_command(const clang::tooling::CompileCommand& command) {
    return {
        .command =
            {
                .working_directory = std::filesystem::path{command.Directory},
                .arguments = command.CommandLine,
            },
        .error = {},
    };
}

void remove_compile_only_arguments(clang::tooling::CompileCommand& command) {
    std::erase_if(command.CommandLine,
                  [](const std::string& argument) { return argument == "/c" || argument == "-c"; });
}

} // namespace

CallableInventoryResult inventory_callables(const std::filesystem::path& build_path,
                                            const std::filesystem::path& source_path,
                                            std::uintmax_t size_limit_bytes) {
    CallableInventoryResult result;
    auto lookup = detail::find_compilation_command(build_path, source_path);
    if (!lookup) {
        result.compilation.error = lookup.error;
        result.error = lookup.error;
        return result;
    }

    result.compilation = public_command(lookup.command);
    remove_compile_only_arguments(lookup.command);
    SingleCommandDatabase database{std::move(lookup.command)};
    clang::tooling::ClangTool tool{database, {detail::path_to_utf8(source_path)}};
    CollectingDiagnosticConsumer diagnostic_consumer{result.diagnostics};
    tool.setDiagnosticConsumer(&diagnostic_consumer);
    CallableActionFactory action_factory{size_limit_bytes, result.callables, result.dependencies,
                                         result.includes};
    if (tool.run(&action_factory) != 0) {
        result.callables.clear();
        result.dependencies.clear();
        result.includes.clear();
        result.error = "Clang frontend failed to analyze: " + detail::path_to_utf8(source_path);
    }

    std::ranges::sort(result.callables, {}, &CallableDefinition::begin_offset);
    return result;
}

} // namespace codesplit::analysis

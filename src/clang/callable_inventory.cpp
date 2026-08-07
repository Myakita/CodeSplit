#include "codesplit/analysis/callable_inventory.hpp"
#include "compilation_database_internal.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Lex/Lexer.h>
#include <clang/Tooling/Tooling.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace codesplit::analysis {
namespace {

void add_constraint(CallableDefinition& callable, CallableConstraint constraint) {
    callable.constraints.push_back(constraint);
}

class CallableVisitor : public clang::RecursiveASTVisitor<CallableVisitor> {
  public:
    CallableVisitor(clang::SourceManager& source_manager,
                    const clang::LangOptions& language_options, std::uintmax_t size_limit_bytes,
                    std::vector<CallableDefinition>& callables)
        : source_manager_{source_manager}, language_options_{language_options},
          size_limit_bytes_{size_limit_bytes}, callables_{callables} {}

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
        CallableDefinition callable{
            .kind = method == nullptr ? CallableKind::free_function : CallableKind::method,
            .qualified_name = declaration->getQualifiedNameAsString(),
        };

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
};

class CallableConsumer : public clang::ASTConsumer {
  public:
    CallableConsumer(clang::SourceManager& source_manager,
                     const clang::LangOptions& language_options, std::uintmax_t size_limit_bytes,
                     std::vector<CallableDefinition>& callables)
        : visitor_{source_manager, language_options, size_limit_bytes, callables} {}

    void HandleTranslationUnit(clang::ASTContext& context) override {
        visitor_.TraverseDecl(context.getTranslationUnitDecl());
    }

  private:
    CallableVisitor visitor_;
};

class CallableAction : public clang::ASTFrontendAction {
  public:
    CallableAction(std::uintmax_t size_limit_bytes, std::vector<CallableDefinition>& callables)
        : size_limit_bytes_{size_limit_bytes}, callables_{callables} {}

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler,
                                                          llvm::StringRef) override {
        return std::make_unique<CallableConsumer>(
            compiler.getSourceManager(), compiler.getLangOpts(), size_limit_bytes_, callables_);
    }

  private:
    std::uintmax_t size_limit_bytes_;
    std::vector<CallableDefinition>& callables_;
};

class CallableActionFactory : public clang::tooling::FrontendActionFactory {
  public:
    CallableActionFactory(std::uintmax_t size_limit_bytes,
                          std::vector<CallableDefinition>& callables)
        : size_limit_bytes_{size_limit_bytes}, callables_{callables} {}

    std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<CallableAction>(size_limit_bytes_, callables_);
    }

  private:
    std::uintmax_t size_limit_bytes_;
    std::vector<CallableDefinition>& callables_;
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
    };
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
    SingleCommandDatabase database{std::move(lookup.command)};
    clang::tooling::ClangTool tool{database, {detail::path_to_utf8(source_path)}};
    CallableActionFactory action_factory{size_limit_bytes, result.callables};
    if (tool.run(&action_factory) != 0) {
        result.callables.clear();
        result.error = "Clang frontend failed to analyze: " + detail::path_to_utf8(source_path);
    }

    std::ranges::sort(result.callables, {}, &CallableDefinition::begin_offset);
    return result;
}

} // namespace codesplit::analysis

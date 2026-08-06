#include "codesplit/analysis/compilation_database.hpp"

namespace codesplit::analysis {

CompilationCommandResult load_compilation_command(const std::filesystem::path&,
                                                  const std::filesystem::path&) {
    return {.error = "Clang LibTooling support is not available in this build."};
}

} // namespace codesplit::analysis

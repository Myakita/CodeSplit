#include "codesplit/analysis/compilation_database.hpp"

#include <iostream>

int main() {
    const auto result = codesplit::analysis::load_compilation_command("build", "source.cpp");
    if (!result && result.error == "Clang LibTooling support is not available in this build.") {
        std::cout << "Compilation database unavailability reported.\n";
        return 0;
    }

    std::cerr << "FAILED: unavailable LibTooling support should produce a stable error.\n";
    return 1;
}
